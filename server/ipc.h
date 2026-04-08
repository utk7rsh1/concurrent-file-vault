#pragma once
#include "../common/protocol.h"

int              ipc_init();
int              ipc_send_index(const char *filename, int version, const char *username);
VersionManifest *ipc_get_manifest();
void             ipc_notify_conflict(const char *filename, const char *username, const char *diff);
void             ipc_shm_lock();
void             ipc_shm_unlock();
void             ipc_cleanup();
