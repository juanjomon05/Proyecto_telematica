#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include "../protocol/protocol.h"
#include "logger.h"

#define MAX_CLIENTES 20
#define MAX_PARTIDAS (MAX_CLIENTES / 2)  // siempre alcanza para todos los clientes

typedef struct {
    int activo;
    int fd;
    char nickname[20];
    char email[40];
    int partida_id;  // -1 si no esta en una partida
    int slot;        // 1 o 2 dentro de la partida
} Cliente;

typedef struct {
    int activa;
    int jugador[2];  // indices en clientes[]
} Partida;

Cliente clientes[MAX_CLIENTES];
Partida partidas[MAX_PARTIDAS];
int en_espera = -1;  // indice del cliente que espera pareja, -1 si nadie
pthread_mutex_t estado_mutex = PTHREAD_MUTEX_INITIALIZER;

void enviar_error(int fd, uint8_t codigo) {
    Message msg;
    init_message(&msg, MSG_ERROR, 0);
    ErrorPayload ep;
    ep.error_code = codigo;  // 1 = servidor lleno, 2 = oponente desconectado
    memcpy(msg.payload, &ep, sizeof(ep));
    send_message(fd, &msg);
}

// Registra al cliente. Devuelve su id (indice en clientes[]) o -1 si no hay cupo.
int registrar_cliente(int client_fd, Message *msg_in) {
    RegisterPayload reg;
    memcpy(&reg, msg_in->payload, sizeof(reg));
    reg.nickname[sizeof(reg.nickname) - 1] = '\0';
    reg.email[sizeof(reg.email) - 1] = '\0';

    log_msg("Registro recibido: nickname=%s, email=%s\n", reg.nickname, reg.email);

    pthread_mutex_lock(&estado_mutex);

    int id = -1;
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (!clientes[i].activo) { id = i; break; }
    }

    if (id == -1) {
        enviar_error(client_fd, 1);
        pthread_mutex_unlock(&estado_mutex);
        log_msg("Registro rechazado: servidor lleno\n");
        return -1;
    }

    memset(&clientes[id], 0, sizeof(Cliente));
    clientes[id].activo = 1;
    clientes[id].fd = client_fd;
    clientes[id].partida_id = -1;
    strncpy(clientes[id].nickname, reg.nickname, sizeof(clientes[id].nickname) - 1);
    strncpy(clientes[id].email, reg.email, sizeof(clientes[id].email) - 1);

    Message msg_out;
    init_message(&msg_out, REGISTER_OK, msg_in->seq);
    RegisterOkPayload ok;
    ok.player_id = id;
    memcpy(msg_out.payload, &ok, sizeof(ok));
    send_message(client_fd, &msg_out);

    pthread_mutex_unlock(&estado_mutex);
    log_msg("Cliente registrado con player_id=%d\n", id);
    return id;
}

// Avisa a 'para' que empezo la partida y quien es su oponente.
// Se llama con estado_mutex tomado.
void enviar_game_start(int para, int oponente) {
    Message msg;
    init_message(&msg, GAME_START, 0);
    GameStartPayload g;
    memset(&g, 0, sizeof(g));
    g.slot = clientes[para].slot;
    strncpy(g.opponent_nick, clientes[oponente].nickname, sizeof(g.opponent_nick) - 1);
    memcpy(msg.payload, &g, sizeof(g));
    send_message(clientes[para].fd, &msg);
}

// Si hay alguien esperando, crea la partida; si no, el cliente queda en espera.
void buscar_pareja(int id) {
    pthread_mutex_lock(&estado_mutex);

    if (en_espera == -1) {
        en_espera = id;
        log_msg("Cliente %d (%s) en espera de pareja\n", id, clientes[id].nickname);
        pthread_mutex_unlock(&estado_mutex);
        return;
    }

    int rival = en_espera;
    en_espera = -1;

    int p = 0;
    while (partidas[p].activa) p++;  // siempre hay una libre (ver MAX_PARTIDAS)

    partidas[p].activa = 1;
    partidas[p].jugador[0] = rival;
    partidas[p].jugador[1] = id;
    clientes[rival].partida_id = p;
    clientes[rival].slot = 1;
    clientes[id].partida_id = p;
    clientes[id].slot = 2;

    log_msg("Partida %d creada: %s vs %s\n", p,
           clientes[rival].nickname, clientes[id].nickname);

    enviar_game_start(rival, id);
    enviar_game_start(id, rival);

    pthread_mutex_unlock(&estado_mutex);
}

// Libera al cliente y avisa a su rival si estaba en partida.
// Se envia con el mutex tomado para que el fd del rival no se cierre mientras se le escribe.
void desconectar_cliente(int id) {
    pthread_mutex_lock(&estado_mutex);

    if (en_espera == id) en_espera = -1;

    int p = clientes[id].partida_id;
    if (p >= 0 && partidas[p].activa) {
        int rival = (partidas[p].jugador[0] == id) ? partidas[p].jugador[1]
                                                   : partidas[p].jugador[0];
        partidas[p].activa = 0;
        clientes[rival].partida_id = -1;
        enviar_error(clientes[rival].fd, 2);
        log_msg("Partida %d terminada: %s se desconecto\n", p, clientes[id].nickname);
    }

    clientes[id].activo = 0;
    pthread_mutex_unlock(&estado_mutex);
}

void *atender_cliente(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    log_msg("Hilo iniciado para cliente (fd=%d)\n", client_fd);

    Message msg_in;
    if (recv_message(client_fd, &msg_in) <= 0 || msg_in.type != REGISTER_REQ) {
        log_msg("Cliente (fd=%d) no se registro, cerrando\n", client_fd);
        close(client_fd);
        return NULL;
    }

    int id = registrar_cliente(client_fd, &msg_in);
    if (id < 0) {
        close(client_fd);
        return NULL;
    }

    buscar_pareja(id);

    // Loop principal: el cliente sigue conectado mientras espera y mientras juega
    while (recv_message(client_fd, &msg_in) > 0) {
        if (msg_in.type == MOVE_REQ) {
            log_msg("MOVE_REQ de cliente %d\n", id);
            // TODO RF-10: procesar el movimiento
        }
    }

    log_msg("Cliente %d desconectado\n", id);
    desconectar_cliente(id);
    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        log_msg("Uso: %s <PORT> <LogFile>\n", argv[0]);
        exit(1);
    }

    signal(SIGPIPE, SIG_IGN);

    int port = atoi(argv[1]);
    log_init(argv[2]);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(1);
    }

    log_msg("Servidor escuchando en el puerto %d...\n", port);

    while (1) {
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, NULL, NULL);

        if (*client_fd < 0) {
            perror("accept");
            free(client_fd);
            continue;
        }

        log_msg("Cliente conectado (fd=%d)\n", *client_fd);

        pthread_t tid;
        pthread_create(&tid, NULL, atender_cliente, client_fd);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
