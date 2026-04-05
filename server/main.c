#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "auth.h"
#include "../common/protocol.h"

typedef struct {
    int  sockfd;
    char username[64];
    Role role;
    int  authenticated;
} ClientCtx;

static ssize_t recv_full(int fd, void *buf, size_t len) {
    size_t received = 0;
    while (received < len) {
        ssize_t n = recv(fd, (char *)buf + received, len - received, 0);
        if (n <= 0) return n == 0 ? (ssize_t)received : -1;
        received += n;
    }
    return (ssize_t)received;
}

static void send_response(int fd, int status, const char *msg, long filesize, int version) {
    Response res;
    memset(&res, 0, sizeof(res));
    res.status   = status;
    res.filesize = filesize;
    res.version  = version;
    if (msg) strncpy(res.message, msg, sizeof(res.message) - 1);
    send(fd, &res, sizeof(res), 0);
}

static void handle_auth(ClientCtx *ctx, Request *req) {
    Role role;
    if (auth_check(req->username, req->password, &role) < 0) {
        send_response(ctx->sockfd, -1, "Authentication failed: invalid credentials", 0, 0);
        return;
    }
    ctx->role          = role;
    ctx->authenticated = 1;
    strncpy(ctx->username, req->username, 63);

    char msg[128];
    snprintf(msg, sizeof(msg), "Authenticated as '%s' with role: %s",
             ctx->username, auth_role_name(ctx->role));
    send_response(ctx->sockfd, 0, msg, 0, 0);
}

static void *client_handler(void *arg) {
    ClientCtx *ctx = (ClientCtx *)arg;
    printf("[SERVER] Client connected (fd=%d)\n", ctx->sockfd);
    Request req;

    while (1) {
        ssize_t n = recv_full(ctx->sockfd, &req, sizeof(req));
        if (n < (ssize_t)sizeof(req)) break;
        if (req.type == CMD_QUIT) break;

        if (req.type == CMD_AUTH) {
            handle_auth(ctx, &req);
            continue;
        }

        if (!ctx->authenticated) {
            send_response(ctx->sockfd, -1, "Not authenticated. Use: auth <user> <pass>", 0, 0);
            continue;
        }

        send_response(ctx->sockfd, -1, "Command not implemented yet", 0, 0);
    }

    printf("[SERVER] Client disconnected\n");
    close(ctx->sockfd);
    free(ctx);
    return NULL;
}

int main() {
    auth_init();

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

        ClientCtx *ctx = calloc(1, sizeof(ClientCtx));
        ctx->sockfd = cli_fd;
        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, ctx);
        pthread_detach(tid);
    }
    close(server_fd);
    return 0;
}
