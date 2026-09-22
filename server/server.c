#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
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

void manejar_registro(int client_fd, Message *msg_in) {
    RegisterPayload reg;
    memcpy(&reg, msg_in->payload, sizeof(RegisterPayload));

    printf("Registro recibido: nickname=%s, email=%s\n", reg.nickname, reg.email);

    uint8_t nuevo_id = total_clientes;
    clientes[total_clientes].player_id = nuevo_id;
    strncpy(clientes[total_clientes].nickname, reg.nickname, sizeof(reg.nickname));
    strncpy(clientes[total_clientes].email, reg.email, sizeof(reg.email));
    clientes[total_clientes].activo = 1;
    total_clientes++;

    Message msg_out;
    init_message(&msg_out, REGISTER_OK, msg_in->seq);
    RegisterOkPayload ok_payload;
    ok_payload.player_id = nuevo_id;
    memcpy(msg_out.payload, &ok_payload, sizeof(RegisterOkPayload));

    send_message(client_fd, &msg_out);
    printf("Cliente registrado con player_id=%d\n", nuevo_id);
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

    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("accept");
        exit(1);
    }

    printf("Cliente conectado.\n");

    Message msg_in;
    int bytes = recv_message(client_fd, &msg_in);
    if (bytes <= 0) {
        printf("Cliente desconectado.\n");
        close(client_fd);
        close(server_fd);
        return 0;
    }

    if (msg_in.type == REGISTER_REQ) {
        manejar_registro(client_fd, &msg_in);
    }

    close(client_fd);
    close(server_fd);
    return 0;
}