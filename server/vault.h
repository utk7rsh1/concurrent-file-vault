#pragma once
#include "../common/protocol.h"

int  vault_init();
int  vault_push(const char *filename, const char *data, long size,
                const char *username, int base_version,
                int *new_version, char *conflict_diff, int diff_buf_len);
int  vault_pull(const char *filename, int version,
                char **data, long *size, int *actual_version);
int  vault_list(char *output, int max_len);
int  vault_get_latest_version(const char *filename);
int  vault_delete(const char *filename);
