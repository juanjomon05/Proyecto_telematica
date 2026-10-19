#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <arpa/inet.h>
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

void mostrar_estado(Message *msg) {
    StatePayload s;
    memcpy(&s, msg->payload, sizeof(s));
    // \r reescribe la misma linea en cada STATE
    printf("\rPelota: (%d,%d) | Paleta1: %d | Paleta2: %d | Score: %d-%d   ",
           (int)ntohl(s.ball_x), (int)ntohl(s.ball_y),
           (int)ntohl(s.paddle1_y), (int)ntohl(s.paddle2_y),
           s.score1, s.score2);
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
    if (recv_message(sock, &msg_in) <= 0 || msg_in.type != REGISTER_OK) {
        printf("Registro fallido.\n");
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
            } else if (msg_in.type == MSG_ERROR) {
                ErrorPayload ep;
                memcpy(&ep, msg_in.payload, sizeof(ep));
                if (ep.error_code == 2) printf("\nEl oponente se desconecto.\n");
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
