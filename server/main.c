#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../common/protocol.h"

static void *client_handler(void *arg) {
    int fd = *(int *)arg;
    free(arg);
    printf("[SERVER] Thread handling client fd=%d\n", fd);
    close(fd);
    return NULL;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); return 1;
    }
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen"); return 1;
    }

    printf("[SERVER] P2P File Vault running on port %d\n", SERVER_PORT);

    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int cli_fd = accept(server_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (cli_fd < 0) continue;

        int *pfd = malloc(sizeof(int));
        *pfd = cli_fd;
        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, pfd);
        pthread_detach(tid);
    }
    close(server_fd);
    return 0;
}
