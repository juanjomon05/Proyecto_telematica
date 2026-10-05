#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include "../protocol/protocol.h"

#define MAX_CLIENTES 20

typedef struct {
    uint8_t player_id;
    char nickname[20];
    char email[40];
    int activo;
} Cliente;

Cliente clientes[MAX_CLIENTES];
int total_clientes = 0;
pthread_mutex_t clientes_mutex = PTHREAD_MUTEX_INITIALIZER;

void manejar_registro(int client_fd, Message *msg_in) {
    RegisterPayload reg;
    memcpy(&reg, msg_in->payload, sizeof(RegisterPayload));
    reg.nickname[sizeof(reg.nickname) - 1] = '\0';
    reg.email[sizeof(reg.email) - 1] = '\0';

    printf("Registro recibido: nickname=%s, email=%s\n", reg.nickname, reg.email);

    pthread_mutex_lock(&clientes_mutex);
    if (total_clientes >= MAX_CLIENTES) {
        pthread_mutex_unlock(&clientes_mutex);
        Message err;
        init_message(&err, MSG_ERROR, msg_in->seq);
        ErrorPayload ep = { 1 }; // 1 = servidor lleno
        memcpy(err.payload, &ep, sizeof(ep));
        send_message(client_fd, &err);
        printf("Registro rechazado: servidor lleno\n");
        return;
    }
    uint8_t nuevo_id = total_clientes;
    clientes[total_clientes].player_id = nuevo_id;
    strncpy(clientes[total_clientes].nickname, reg.nickname, sizeof(reg.nickname));
    strncpy(clientes[total_clientes].email, reg.email, sizeof(reg.email));
    clientes[total_clientes].activo = 1;
    total_clientes++;
    pthread_mutex_unlock(&clientes_mutex);

    Message msg_out;
    init_message(&msg_out, REGISTER_OK, msg_in->seq);
    RegisterOkPayload ok_payload;
    ok_payload.player_id = nuevo_id;
    memcpy(msg_out.payload, &ok_payload, sizeof(RegisterOkPayload));

    send_message(client_fd, &msg_out);
    printf("Cliente registrado con player_id=%d\n", nuevo_id);
}

void *atender_cliente(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    printf("Hilo iniciado para cliente (fd=%d)\n", client_fd);

    Message msg_in;
    int bytes = recv_message(client_fd, &msg_in);

    if (bytes <= 0) {
        printf("Cliente desconectado (fd=%d)\n", client_fd);
        close(client_fd);
        return NULL;
    }

    if (msg_in.type == REGISTER_REQ) {
        manejar_registro(client_fd, &msg_in);
    }

    // TODO: aqui va el loop del juego en vez de cerrar
    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <PORT> <LogFile>\n", argv[0]);
        exit(1);
    }

    signal(SIGPIPE, SIG_IGN);

    int port = atoi(argv[1]);

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

    printf("Servidor escuchando en el puerto %d...\n", port);

    while (1) {
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, NULL, NULL);

        if (*client_fd < 0) {
            perror("accept");
            free(client_fd);
            continue;
        }

        printf("Cliente conectado (fd=%d)\n", *client_fd);

        pthread_t tid;
        pthread_create(&tid, NULL, atender_cliente, client_fd);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
