#pragma once
#include "../common/protocol.h"

int              ipc_init();
VersionManifest *ipc_get_manifest();
void             ipc_shm_lock();
void             ipc_shm_unlock();
void             ipc_cleanup();
