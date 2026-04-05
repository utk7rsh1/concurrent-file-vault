#include "auth.h"
#include <string.h>

static User users[] = {
    {"alice",   "alice123",   OWNER},
    {"bob",     "bob123",     CONTRIBUTOR},
    {"charlie", "charlie123", CONTRIBUTOR},
    {"guest1",  "guest111",   VIEWER},
    {"guest2",  "guest222",   VIEWER},
};

static const int user_count = 5;

void auth_init() {}

int auth_check(const char *username, const char *password, Role *out_role) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 &&
            strcmp(users[i].password, password) == 0) {
            *out_role = users[i].role;
            return 0;
        }
    }
    return -1;
}

int auth_has_permission(Role role, CmdType cmd) {
    if (cmd == CMD_AUTH || cmd == CMD_QUIT || cmd == CMD_LIST || cmd == CMD_PULL)
        return 1;
    if (cmd == CMD_PUSH)
        return role == OWNER || role == CONTRIBUTOR;
    if (cmd == CMD_STATUS)
        return role == OWNER || role == CONTRIBUTOR;
    if (cmd == CMD_DELETE || cmd == CMD_LOGS)
        return role == OWNER;
    return 0;
}

const char *auth_role_name(Role role) {
    switch (role) {
        case OWNER:       return "owner";
        case CONTRIBUTOR: return "contributor";
        case VIEWER:      return "viewer";
        default:          return "unknown";
    }
}
