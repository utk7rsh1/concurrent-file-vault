#include "ipc.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static int              shmid    = -1;
static VersionManifest *manifest = NULL;
static sem_t           *shm_sem  = NULL;

int ipc_init() {
    shmid = shmget(SHM_KEY, sizeof(VersionManifest), IPC_CREAT | 0666);
    if (shmid < 0) return -1;

    manifest = (VersionManifest *)shmat(shmid, NULL, 0);
    if (manifest == (void *)-1) return -1;
    memset(manifest, 0, sizeof(VersionManifest));

    sem_unlink(SEM_NAME);
    shm_sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (shm_sem == SEM_FAILED) return -1;

    return 0;
}

VersionManifest *ipc_get_manifest() {
    return manifest;
}

void ipc_shm_lock() {
    if (shm_sem && shm_sem != SEM_FAILED)
        sem_wait(shm_sem);
}

void ipc_shm_unlock() {
    if (shm_sem && shm_sem != SEM_FAILED)
        sem_post(shm_sem);
}

void ipc_cleanup() {
    if (manifest && manifest != (void *)-1) shmdt(manifest);
    if (shmid >= 0) shmctl(shmid, IPC_RMID, NULL);
    if (shm_sem && shm_sem != SEM_FAILED) {
        sem_close(shm_sem);
        sem_unlink(SEM_NAME);
    }
}
