#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "../protocol/protocol.h"

static struct termios term_original;
static int term_modificada = 0;

// Lee teclas sin esperar Enter y sin mostrarlas en pantalla
void activar_modo_tecla() {
    if (!isatty(STDIN_FILENO)) return;
    tcgetattr(STDIN_FILENO, &term_original);
    struct termios t = term_original;
    t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    term_modificada = 1;
}

void restaurar_terminal() {
    if (term_modificada) tcsetattr(STDIN_FILENO, TCSANOW, &term_original);
}

void enviar_movimiento(int sock, uint8_t direction) {
    Message msg;
    init_message(&msg, MOVE_REQ, 0);
    MovePayload mv;
    mv.direction = direction;
    memcpy(msg.payload, &mv, sizeof(mv));
    send_message(sock, &msg);
}

// Campo logico del servidor (ver server/game.h) y tamano del dibujo en la terminal
#define CAMPO_ANCHO  800
#define CAMPO_ALTO   600
#define PALETA_ALTO  100
#define COLS 60
#define FILAS 20

static int mi_slot = 0;
static char mi_oponente[20] = "";

// Dibuja el campo en ASCII. \033[H lleva el cursor arriba para redibujar sobre lo anterior.
void mostrar_estado(Message *msg) {
    StatePayload s;
    memcpy(&s, msg->payload, sizeof(s));
    int bx = (int)ntohl(s.ball_x),    by = (int)ntohl(s.ball_y);
    int p1 = (int)ntohl(s.paddle1_y), p2 = (int)ntohl(s.paddle2_y);

    char campo[FILAS][COLS + 1];
    for (int f = 0; f < FILAS; f++) {
        memset(campo[f], ' ', COLS);
        campo[f][COLS] = '\0';
        campo[f][COLS / 2] = ':';  // red central
    }

    // Paletas: columna 1 (jugador 1) y columna COLS-2 (jugador 2)
    int f1a = p1 * FILAS / CAMPO_ALTO, f1b = (p1 + PALETA_ALTO) * FILAS / CAMPO_ALTO;
    int f2a = p2 * FILAS / CAMPO_ALTO, f2b = (p2 + PALETA_ALTO) * FILAS / CAMPO_ALTO;
    for (int f = f1a; f <= f1b && f < FILAS; f++) campo[f][1] = '|';
    for (int f = f2a; f <= f2b && f < FILAS; f++) campo[f][COLS - 2] = '|';

    // Pelota
    int cx = bx * COLS / CAMPO_ANCHO, cy = by * FILAS / CAMPO_ALTO;
    if (cx >= 0 && cx < COLS && cy >= 0 && cy < FILAS) campo[cy][cx] = 'O';

    printf("\033[H");
    printf("PONG  |  Tu eres el jugador %d (%s) vs %s\n", mi_slot,
           (mi_slot == 1) ? "izquierda" : "derecha", mi_oponente);
    printf("Marcador: J1 %d - %d J2\n", s.score1, s.score2);
    printf("+"); for (int c = 0; c < COLS; c++) printf("-"); printf("+\n");
    for (int f = 0; f < FILAS; f++) printf("|%s|\n", campo[f]);
    printf("+"); for (int c = 0; c < COLS; c++) printf("-"); printf("+\n");
    printf("w=arriba  s=abajo  espacio=quieto  q=salir\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Uso: %s <SERVER_IP> <PORT> <nickname> <email>\n", argv[0]);
        exit(1);
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); exit(1); }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        exit(1);
    }

    int nodelay = 1;  // mensajes pequenos y frecuentes: evita el retardo de Nagle
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    printf("Conectado al servidor.\n");

    Message msg_out;
    init_message(&msg_out, REGISTER_REQ, 1);
    RegisterPayload reg;
    memset(&reg, 0, sizeof(reg));
    strncpy(reg.nickname, argv[3], sizeof(reg.nickname) - 1);
    strncpy(reg.email, argv[4], sizeof(reg.email) - 1);
    memcpy(msg_out.payload, &reg, sizeof(reg));
    send_message(sock, &msg_out);

    Message msg_in;
    memset(&msg_in, 0, sizeof(msg_in));
    if (recv_message(sock, &msg_in) <= 0 || msg_in.type != REGISTER_OK) {
        printf("Registro fallido");
        if (msg_in.type == MSG_ERROR) {
            ErrorPayload ep;
            memcpy(&ep, msg_in.payload, sizeof(ep));
            if (ep.error_code == ERR_SERVIDOR_LLENO) printf(": el servidor esta lleno");
            else if (ep.error_code == ERR_DATOS_INVALIDOS) printf(": nickname vacio o email invalido");
            else if (ep.error_code == ERR_NICK_EN_USO) printf(": ese nickname ya esta en uso");
            else printf(" (codigo %d)", ep.error_code);
        }
        printf(".\n");
        close(sock);
        return 0;
    }

    RegisterOkPayload ok;
    memcpy(&ok, msg_in.payload, sizeof(ok));
    printf("Registrado como %s (player_id=%d). Esperando pareja...\n", reg.nickname, ok.player_id);

    if (recv_message(sock, &msg_in) <= 0 || msg_in.type != GAME_START) {
        printf("No se pudo iniciar la partida.\n");
        close(sock);
        return 0;
    }

    GameStartPayload g;
    memcpy(&g, msg_in.payload, sizeof(g));
    printf("Partida iniciada: eres el jugador %d (%s), oponente=%s\n",
           g.slot, (g.slot == 1) ? "izquierda" : "derecha", g.opponent_nick);
    printf("Controles: w=arriba, s=abajo, espacio=quieto, q=salir\n");

    mi_slot = g.slot;
    strncpy(mi_oponente, g.opponent_nick, sizeof(mi_oponente) - 1);
    printf("\033[2J");  // limpia la pantalla una vez antes de empezar a dibujar

    activar_modo_tecla();

    // select() espera a la vez el teclado y el socket, sin hilos
    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(sock, &fds);

        if (select(sock + 1, &fds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &fds)) {
            char c;
            if (read(STDIN_FILENO, &c, 1) <= 0 || c == 'q') break;
            if (c == 'w') enviar_movimiento(sock, 0);
            else if (c == 's') enviar_movimiento(sock, 1);
            else if (c == ' ') enviar_movimiento(sock, 2);
        }

        if (FD_ISSET(sock, &fds)) {
            if (recv_message(sock, &msg_in) <= 0) {
                printf("\nServidor cerro la conexion.\n");
                break;
            }
            if (msg_in.type == STATE_UPDATE) {
                mostrar_estado(&msg_in);
            } else if (msg_in.type == GAME_OVER) {
                GameOverPayload go;
                memcpy(&go, msg_in.payload, sizeof(go));
                printf("\nFin de la partida. Gano el jugador %d: %s\n", go.winner_id,
                       (go.winner_id == mi_slot) ? "¡ganaste!" : "gano tu oponente.");
                break;
            } else if (msg_in.type == MSG_ERROR) {
                ErrorPayload ep;
                memcpy(&ep, msg_in.payload, sizeof(ep));
                if (ep.error_code == ERR_OPONENTE_DESCONECTADO) printf("\nEl oponente se desconecto.\n");
                else printf("\nERROR recibido, codigo=%d\n", ep.error_code);
                break;
            }
        }
    }

    restaurar_terminal();
    printf("\n");
    close(sock);
    return 0;
}
