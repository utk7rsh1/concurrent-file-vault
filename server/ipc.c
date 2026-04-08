#include "ipc.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

static int              shmid    = -1;
static int              msgid    = -1;
static VersionManifest *manifest = NULL;
static sem_t           *shm_sem  = NULL;

int ipc_init() {
    shmid = shmget(SHM_KEY, sizeof(VersionManifest), IPC_CREAT | 0666);
    if (shmid < 0) return -1;

    manifest = (VersionManifest *)shmat(shmid, NULL, 0);
    if (manifest == (void *)-1) return -1;
    memset(manifest, 0, sizeof(VersionManifest));

    msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msgid < 0) return -1;

    mkfifo(FIFO_PATH, 0666);
    
    sem_unlink(SEM_NAME);
    shm_sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 1);
    if (shm_sem == SEM_FAILED) return -1;

    return 0;
}

int ipc_send_index(const char *filename, int version, const char *username) {
    IndexMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.mtype = 1;
    strncpy(msg.filename, filename, MAX_FILENAME - 1);
    msg.version   = version;
    msg.timestamp = time(NULL);
    strncpy(msg.username, username, 63);
    return msgsnd(msgid, &msg, sizeof(IndexMsg) - sizeof(long), 0);
}

VersionManifest *ipc_get_manifest() {
    return manifest;
}

void ipc_notify_conflict(const char *filename, const char *username, const char *diff) {
    int fd = open(FIFO_PATH, O_WRONLY | O_NONBLOCK);
    if (fd < 0) return;

    char buf[3200];
    int n = snprintf(buf, sizeof(buf), "CONFLICT|%s|%s|%.2048s", filename, username, diff ? diff : "");
    write(fd, buf, n);
    close(fd);
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
    if (msgid >= 0) msgctl(msgid, IPC_RMID, NULL);
    unlink(FIFO_PATH);
    if (shm_sem && shm_sem != SEM_FAILED) {
        sem_close(shm_sem);
        sem_unlink(SEM_NAME);
    }
}
