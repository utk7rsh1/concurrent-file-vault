#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../common/protocol.h"

static int sockfd = -1;

static ssize_t recv_full(int fd, void *buf, size_t len) {
    size_t received = 0;
    while (received < len) {
        ssize_t n = recv(fd, (char *)buf + received, len - received, 0);
        if (n <= 0) return n == 0 ? (ssize_t)received : -1;
        received += n;
    }
    return (ssize_t)received;
}

static int send_req(Request *req) {
    return (int)send(sockfd, req, sizeof(Request), MSG_NOSIGNAL);
}

static int recv_res(Response *res) {
    ssize_t n = recv_full(sockfd, res, sizeof(Response));
    return (n == (ssize_t)sizeof(Response)) ? (int)n : -1;
}

static void cmd_auth(const char *username, const char *password) {
    Request req;
    memset(&req, 0, sizeof(req));
    req.type = CMD_AUTH;
    strncpy(req.username, username, 63);
    req.username[63] = '\0';
    strncpy(req.password, password, 63);
    req.password[63] = '\0';
    send_req(&req);

    Response res;
    if (recv_res(&res) < 0) { printf("[ERR] Connection lost\n"); return; }
    printf("[%s] %s\n", res.status == 0 ? "OK" : "ERR", res.message);
}

static void cmd_push(const char *remote_name, const char *local_path, int base_version) {
    int fd = open(local_path, O_RDONLY);
    if (fd < 0) { printf("[ERR] Cannot open local file: %s\n", local_path); return; }

    long size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if (size > MAX_FILESIZE) {
        printf("[ERR] File too large (max %d bytes)\n", MAX_FILESIZE);
        close(fd);
        return;
    }

    char *data = malloc(size);
    if (!data) { close(fd); printf("[ERR] Out of memory\n"); return; }
    long rd = 0;
    while (rd < size) {
        ssize_t n = read(fd, data + rd, size - rd);
        if (n <= 0) break;
        rd += n;
    }
    close(fd);

    Request req;
    memset(&req, 0, sizeof(req));
    req.type         = CMD_PUSH;
    req.filesize     = size;
    req.base_version = base_version;
    strncpy(req.filename, remote_name, MAX_FILENAME - 1);
    req.filename[MAX_FILENAME - 1] = '\0';
    send_req(&req);

    Response res;
    if (recv_res(&res) < 0) { printf("[ERR] Connection lost\n"); free(data); return; }
    if (res.status != 0) {
        printf("[%s] %s\n", res.status == -2 ? "CONFLICT" : "ERR", res.message);
        free(data);
        return;
    }

    printf("[SERVER] Ready to receive. Sending %ld bytes...\n", size);
    long sent = 0;
    while (sent < size) {
        ssize_t n = send(sockfd, data + sent, size - sent, MSG_NOSIGNAL);
        if (n <= 0) { printf("[ERR] Send interrupted\n"); break; }
        sent += n;
    }
    free(data);

    if (recv_res(&res) < 0) { printf("[ERR] Connection lost\n"); return; }
    printf("[%s] %s\n", res.status == 0 ? "OK" : "ERR", res.message);
}

static void cmd_pull(const char *remote_name, int version, const char *save_path) {
    Request req;
    memset(&req, 0, sizeof(req));
    req.type         = CMD_PULL;
    req.base_version = version;
    strncpy(req.filename, remote_name, MAX_FILENAME - 1);
    req.filename[MAX_FILENAME - 1] = '\0';
    send_req(&req);

    Response res;
    if (recv_res(&res) < 0) { printf("[ERR] Connection lost\n"); return; }
    if (res.status != 0) { printf("[ERR] %s\n", res.message); return; }

    printf("[OK] %s\n", res.message);

    char *data = malloc(res.filesize + 1);
    if (!data) { printf("[ERR] Out of memory\n"); return; }

    long received = 0;
    while (received < res.filesize) {
        ssize_t n = recv(sockfd, data + received, res.filesize - received, 0);
        if (n <= 0) break;
        received += n;
    }

    const char *out = save_path ? save_path : remote_name;
    int out_fd = creat(out, 0644);
    if (out_fd >= 0) {
        ssize_t w = write(out_fd, data, received);
        close(out_fd);
        if (w < 0) perror("[ERR] write");
        else printf("[OK] Saved to: %s (%ld bytes)\n", out, received);
    } else {
        printf("[ERR] Cannot write to: %s\n", out);
    }
    free(data);
}

static void cmd_list() {
    Request req;
    memset(&req, 0, sizeof(req));
    req.type = CMD_LIST;
    send_req(&req);

    Response res;
    if (recv_res(&res) < 0) { printf("[ERR] Connection lost\n"); return; }
    printf("%s\n", res.message);
}

static void cmd_status() {
    Request req;
    memset(&req, 0, sizeof(req));
    req.type = CMD_STATUS;
    send_req(&req);

    Response res;
    if (recv_res(&res) < 0) { printf("[ERR] Connection lost\n"); return; }
    printf("[%s] %s\n", res.status == 0 ? "OK" : "ERR", res.message);
}

static void cmd_delete(const char *remote_name) {
    Request req;
    memset(&req, 0, sizeof(req));
    req.type = CMD_DELETE;
    strncpy(req.filename, remote_name, MAX_FILENAME - 1);
    req.filename[MAX_FILENAME - 1] = '\0';
    send_req(&req);

    Response res;
    if (recv_res(&res) < 0) { printf("[ERR] Connection lost\n"); return; }
    printf("[%s] %s\n", res.status == 0 ? "OK" : "ERR", res.message);
}

static void cmd_logs() {
    Request req;
    memset(&req, 0, sizeof(req));
    req.type = CMD_LOGS;
    send_req(&req);

    Response res;
    if (recv_res(&res) < 0) { printf("[ERR] Connection lost\n"); return; }
    printf("[%s] %s\n", res.status == 0 ? "OK" : "ERR", res.message);
}

static void print_help() {
    printf("\nAvailable commands:\n");
    printf("  auth   <username> <password>\n");
    printf("  push   <remote_name> <local_file> [base_version]\n");
    printf("  pull   <remote_name> [version] [save_path]\n");
    printf("  delete <remote_name>          (owner only)\n");
    printf("  logs                          (owner only)\n");
    printf("  list\n");
    printf("  status\n");
    printf("  help\n");
    printf("  quit\n\n");
}

int main(int argc, char *argv[]) {
    const char *host = argc > 1 ? argv[1] : "127.0.0.1";
    int         port = argc > 2 ? atoi(argv[2]) : SERVER_PORT;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        printf("[ERR] Invalid address: %s\n", host);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[ERR] Connection failed");
        return 1;
    }

    printf("╔══════════════════════════════════════╗\n");
    printf("║      P2P File Vault Client v1.0      ║\n");
    printf("╚══════════════════════════════════════╝\n");
    printf("Connected to %s:%d\n", host, port);
    print_help();

    char line[1024];
    while (1) {
        printf("vault> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') continue;

        char cmd[32]  = "";
        char a1[256]  = "";
        char a2[256]  = "";
        char a3[256]  = "";
        int  n        = sscanf(line, "%31s %255s %255s %255s", cmd, a1, a2, a3);
        if (n < 1) continue;

        if (strcmp(cmd, "auth") == 0 && n >= 3) {
            cmd_auth(a1, a2);

        } else if (strcmp(cmd, "push") == 0 && n >= 3) {
            int base = (n >= 4) ? atoi(a3) : 0;
            cmd_push(a1, a2, base);

        } else if (strcmp(cmd, "pull") == 0 && n >= 2) {
            int         ver  = (n >= 3 && a2[0] >= '0' && a2[0] <= '9') ? atoi(a2) : 0;
            const char *path = (n >= 4) ? a3 : (n >= 3 && !(a2[0] >= '0' && a2[0] <= '9') ? a2 : NULL);
            cmd_pull(a1, ver, path);

        } else if (strcmp(cmd, "delete") == 0 && n >= 2) {
            cmd_delete(a1);

        } else if (strcmp(cmd, "logs") == 0) {
            cmd_logs();

        } else if (strcmp(cmd, "list") == 0) {
            cmd_list();

        } else if (strcmp(cmd, "status") == 0) {
            cmd_status();

        } else if (strcmp(cmd, "help") == 0) {
            print_help();

        } else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            Request req;
            memset(&req, 0, sizeof(req));
            req.type = CMD_QUIT;
            send_req(&req);
            break;

        } else {
            printf("[ERR] Unknown command. Type 'help' for usage.\n");
        }
    }

    close(sockfd);
    printf("Disconnected.\n");
    return 0;
}
