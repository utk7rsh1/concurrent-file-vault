#pragma once
#include "../common/protocol.h"

typedef struct {
    char username[64];
    char password[64];
    Role role;
} User;

void auth_init();
int  auth_check(const char *username, const char *password, Role *out_role);
int  auth_has_permission(Role role, CmdType cmd);
const char *auth_role_name(Role role);
