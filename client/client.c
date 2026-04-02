#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../common/protocol.h"

static int sockfd = -1;

static void cmd_auth(const char *username, const char *password) {
    printf("[TODO] Authentication for %s:%s\n", username, password);
}

static void cmd_push(const char *remote_name, const char *local_path, int base_version) {
    printf("[TODO] Push %s from %s (base: %d)\n", remote_name, local_path, base_version);
}

static void cmd_pull(const char *remote_name, int version, const char *save_path) {
    printf("[TODO] Pull %s version %d to %s\n", remote_name, version, save_path ? save_path : "default");
}

static void cmd_list() {
    printf("[TODO] List files\n");
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
    printf("Connected (Simulation Mode)\n");
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
    return 0;
}
