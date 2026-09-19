#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include "../protocol/protocol.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <PORT> <LogFile>\n", argv[0]);
        exit(1);
    }

    signal(SIGPIPE, SIG_IGN); // evita crash si un cliente se desconecta abrupto

    int port = atoi(argv[1]);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(1);
    }

    // Permite reusar el puerto rápido si reinicias el servidor
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

    close(client_fd);
    close(server_fd);
    return 0;
}
