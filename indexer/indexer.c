#include "../common/protocol.h"
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <time.h>
#include <unistd.h>

static volatile int running = 1;

static void sig_handler(int sig) {
  (void)sig;
  running = 0;
}

int main() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sig_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  printf("[INDEXER] Starting, waiting for server IPC resources...\n");

  int shmid = -1;
  while (running && shmid < 0) {
    shmid = shmget(SHM_KEY, sizeof(VersionManifest), 0666);
    if (shmid < 0)
      sleep(1);
  }
  if (!running)
    return 0;

  VersionManifest *manifest = (VersionManifest *)shmat(shmid, NULL, 0);
  if (manifest == (void *)-1) {
    perror("shmat");
    return 1;
  }

  int msgid = -1;
  while (running && msgid < 0) {
    msgid = msgget(MSG_KEY, 0666);
    if (msgid < 0)
      sleep(1);
  }
  if (!running) {
    shmdt(manifest);
    return 0;
  }

  sem_t *sem = NULL;
  while (running && (sem == NULL || sem == SEM_FAILED)) {
    sem = sem_open(SEM_NAME, 0);
    if (sem == SEM_FAILED)
      sleep(1);
  }
  if (!running) {
    shmdt(manifest);
    return 0;
  }

  int log_fd = open("vault_index.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
  printf("[INDEXER] Ready. Listening on message queue...\n\n");

  IndexMsg msg;
  while (running) {
    ssize_t ret =
        msgrcv(msgid, &msg, sizeof(IndexMsg) - sizeof(long), 1, MSG_NOERROR);
    if (ret < 0) {
      if (running)
        sleep(1);
      continue;
    }

    sem_wait(sem);

    int found = 0;
    for (int i = 0; i < manifest->file_count; i++) {
      if (strncmp(manifest->files[i].filename, msg.filename, MAX_FILENAME) ==
          0) {
        manifest->files[i].latest_version = msg.version;
        manifest->files[i].last_modified = msg.timestamp;
        strncpy(manifest->files[i].last_user, msg.username, 63);
        manifest->files[i].last_user[63] = '\0';
        found = 1;
        break;
      }
    }

    if (!found && manifest->file_count < MAX_FILES) {
      FileEntry *e = &manifest->files[manifest->file_count++];
      strncpy(e->filename, msg.filename, MAX_FILENAME - 1);
      e->filename[MAX_FILENAME - 1] = '\0';
      e->latest_version = msg.version;
      e->last_modified = msg.timestamp;
      strncpy(e->last_user, msg.username, 63);
      e->last_user[63] = '\0';
    }

    sem_post(sem);

    char timebuf[32] = "unknown";
    struct tm *tm_info = localtime(&msg.timestamp);
    if (tm_info)
      strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);

    printf("[INDEXER] Indexed: %-30s  v%d  by: %-15s  at: %s\n", msg.filename,
           msg.version, msg.username, timebuf);

    if (log_fd >= 0) {
      char logbuf[512];
      int lb = snprintf(logbuf, sizeof(logbuf),
                        "[%s] file=%-30s version=v%d user=%s\n", timebuf,
                        msg.filename, msg.version, msg.username);
      ssize_t w = write(log_fd, logbuf, lb);
      if (w < 0)
        perror("[INDEXER] write log");
    }
  }

  if (log_fd >= 0)
    close(log_fd);
  shmdt(manifest);
  sem_close(sem);
  printf("[INDEXER] Shutdown complete.\n");
  return 0;
}
