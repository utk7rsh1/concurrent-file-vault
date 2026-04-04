#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../common/protocol.h"

static int sockfd = -1;

static void cmd_auth(const char *username, const char *password) {
    Request req;
    memset(&req, 0, sizeof(req));
    req.type = CMD_AUTH;
    strncpy(req.username, username, 63);
    strncpy(req.password, password, 63);
    send(sockfd, &req, sizeof(req), 0);
    printf("Sent auth request for %s\n", username);
}

static void cmd_push(const char *remote_name, const char *local_path, int base_version) {
    printf("[TODO] Push %s\n", remote_name);
}

static void cmd_pull(const char *remote_name, int version, const char *save_path) {
    printf("[TODO] Pull %s\n", remote_name);
}

static void cmd_list() {
    printf("[TODO] List\n");
}

static void print_help() {
    printf("\nAvailable commands:\n");
    printf("  auth   <username> <password>\n");
    printf("  push   <remote_name> <local_file> [base_version]\n");
    printf("  pull   <remote_name> [version] [save_path]\n");
    printf("  list\n");
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
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[ERR] Connection failed");
        return 1;
    }

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
            int ver  = (n >= 3 && a2[0] >= '0' && a2[0] <= '9') ? atoi(a2) : 0;
            const char *path = (n >= 4) ? a3 : (n >= 3 && !(a2[0] >= '0' && a2[0] <= '9') ? a2 : NULL);
            cmd_pull(a1, ver, path);
        } else if (strcmp(cmd, "list") == 0) {
            cmd_list();
        } else if (strcmp(cmd, "help") == 0) {
            print_help();
        } else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            break;
        } else {
            printf("[ERR] Unknown command. Type 'help' for usage.\n");
        }
    }
    close(sockfd);
    return 0;
}
