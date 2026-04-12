#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include "auth.h"
#include "vault.h"
#include "ipc.h"
#include "../common/protocol.h"

static sem_t            conn_sem;
static volatile int     running = 1;

static pthread_mutex_t  stats_mutex = PTHREAD_MUTEX_INITIALIZER;
static int              active_clients = 0;

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
    send(fd, &res, sizeof(res), MSG_NOSIGNAL);
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

static void handle_push(ClientCtx *ctx, Request *req) {
    if (!auth_has_permission(ctx->role, CMD_PUSH)) {
        send_response(ctx->sockfd, -1, "Permission denied: your role cannot push files", 0, 0);
        return;
    }
    send_response(ctx->sockfd, 0, "READY", 0, 0);

    char *data = malloc(req->filesize);
    long received = 0;
    while (received < req->filesize) {
        ssize_t n = recv(ctx->sockfd, data + received, req->filesize - received, 0);
        if (n <= 0) break;
        received += n;
    }

    int  new_version    = 0;
    char conflict_diff[2048] = "";
    int  ret = vault_push(req->filename, data, received, ctx->username,
                          req->base_version, &new_version, conflict_diff, sizeof(conflict_diff));
    free(data);

    if (ret == -2) {
        ipc_notify_conflict(req->filename, ctx->username, conflict_diff);
        char msg[2600];
        snprintf(msg, sizeof(msg), "CONFLICT: server version differs from your base.\nDiff:\n%.2048s", conflict_diff);
        send_response(ctx->sockfd, -2, msg, 0, 0);
    } else if (ret == 0) {
        ipc_send_index(req->filename, new_version, ctx->username);
        char msg[128];
        snprintf(msg, sizeof(msg), "Push successful. New version: v%d", new_version);
        send_response(ctx->sockfd, 0, msg, 0, new_version);
    } else {
        send_response(ctx->sockfd, -1, "Push failed: server I/O error", 0, 0);
    }
}

static void handle_pull(ClientCtx *ctx, Request *req) {
    char  *data          = NULL;
    long   size          = 0;
    int    actual_version = 0;

    if (vault_pull(req->filename, req->base_version, &data, &size, &actual_version) != 0) {
        send_response(ctx->sockfd, -1, "File not found in vault", 0, 0);
        return;
    }

    char msg[400];
    snprintf(msg, sizeof(msg), "Sending '%s' version v%d (%ld bytes)", req->filename, actual_version, size);
    send_response(ctx->sockfd, 0, msg, size, actual_version);

    long sent = 0;
    while (sent < size) {
        ssize_t n = send(ctx->sockfd, data + sent, size - sent, MSG_NOSIGNAL);
        if (n <= 0) break;
        sent += n;
    }
    free(data);
}

static void handle_list(ClientCtx *ctx) {
    char output[8192] = "=== Vault Contents ===\n";
    int  base         = strlen(output);
    int  n            = vault_list(output + base, sizeof(output) - base);
    if (n <= 0) snprintf(output + base, sizeof(output) - base, "  (vault is empty)\n");
    send_response(ctx->sockfd, 0, output, 0, 0);
}

static void handle_status(ClientCtx *ctx) {
    ipc_shm_lock();
    VersionManifest *m = ipc_get_manifest();
    char output[4096];
    int  written = 0;

    pthread_mutex_lock(&stats_mutex);
    int clients = active_clients;
    pthread_mutex_unlock(&stats_mutex);

    written += snprintf(output + written, sizeof(output) - written,
                        "=== Server Status ===\n  Active clients: %d\n"
                        "=== Live Manifest (Indexer) ===\n  Total indexed files: %d\n",
                        clients, m ? m->file_count : 0);

    if (m) {
        for (int i = 0; i < m->file_count; i++) {
            written += snprintf(output + written, sizeof(output) - written,
                "  %-30s  v%-3d  by: %-15s\n",
                m->files[i].filename, m->files[i].latest_version,
                m->files[i].last_user);
        }
    }
    ipc_shm_unlock();
    send_response(ctx->sockfd, 0, output, 0, 0);
}

static void *client_handler(void *arg) {
    ClientCtx *ctx = (ClientCtx *)arg;
    Request    req;

    while (running) {
        ssize_t n = recv_full(ctx->sockfd, &req, sizeof(req));
        if (n < (ssize_t)sizeof(req)) break;
        if (req.type == CMD_QUIT) break;
        if (req.type == CMD_AUTH) { handle_auth(ctx, &req); continue; }

        if (!ctx->authenticated) {
            send_response(ctx->sockfd, -1, "Not authenticated. Use: auth <user> <pass>", 0, 0);
            continue;
        }

        switch (req.type) {
            case CMD_PUSH:   handle_push(ctx, &req);   break;
            case CMD_PULL:   handle_pull(ctx, &req);   break;
            case CMD_LIST:   handle_list(ctx);          break;
            case CMD_STATUS: handle_status(ctx);        break;
            default:         send_response(ctx->sockfd, -1, "Unknown command", 0, 0);
        }
    }

    close(ctx->sockfd);
    sem_post(&conn_sem);

    pthread_mutex_lock(&stats_mutex);
    active_clients--;
    pthread_mutex_unlock(&stats_mutex);

    free(ctx);
    return NULL;
}

int main() {
    auth_init();
    vault_init();
    ipc_init();

    sem_init(&conn_sem, 0, MAX_CLIENTS);

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

    while (running) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int cli_fd = accept(server_fd, (struct sockaddr *)&cli_addr, &cli_len);
        if (cli_fd < 0) continue;

        sem_wait(&conn_sem);

        pthread_mutex_lock(&stats_mutex);
        active_clients++;
        pthread_mutex_unlock(&stats_mutex);

        ClientCtx *ctx = calloc(1, sizeof(ClientCtx));
        ctx->sockfd = cli_fd;

        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, ctx);
        pthread_detach(tid);
    }

    close(server_fd);
    ipc_cleanup();
    sem_destroy(&conn_sem);
    return 0;
}
