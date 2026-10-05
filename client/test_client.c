#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "../protocol/protocol.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <SERVER_IP> <PORT>\n", argv[0]);
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
    strncpy(reg.nickname, "JuanTest", sizeof(reg.nickname) - 1);
    strncpy(reg.email, "juan@test.com", sizeof(reg.email) - 1);
    memcpy(msg_out.payload, &reg, sizeof(reg));

    send_message(sock, &msg_out);
    printf("REGISTER_REQ enviado: nickname=%s, email=%s\n", reg.nickname, reg.email);

    Message msg_in;
    int bytes = recv_message(sock, &msg_in);
    if (bytes <= 0) {
        printf("Servidor cerro la conexion.\n");
        close(sock);
        return 0;
    }

    if (msg_in.type == REGISTER_OK) {
        RegisterOkPayload ok;
        memcpy(&ok, msg_in.payload, sizeof(ok));
        printf("REGISTER_OK recibido. player_id=%d\n", ok.player_id);
    } else {
        printf("Respuesta inesperada, type=%d\n", msg_in.type);
    }

    close(sock);
    return 0;
}
