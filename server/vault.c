#include "vault.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

static pthread_rwlock_t vault_rwlock = PTHREAD_RWLOCK_INITIALIZER;

static int apply_fcntl_lock(int fd, short type) {
    struct flock fl;
    fl.l_type   = type;
    fl.l_whence = SEEK_SET;
    fl.l_start  = 0;
    fl.l_len    = 0;
    int ret;
    if (type == F_UNLCK)
        ret = fcntl(fd, F_SETLK, &fl);
    else
        ret = fcntl(fd, F_SETLKW, &fl);
    if (ret < 0)
        perror("[VAULT] fcntl lock");
    return ret;
}

static ssize_t write_full(int fd, const void *buf, size_t count) {
    size_t written = 0;
    while (written < count) {
        ssize_t w = write(fd, (const char *)buf + written, count - written);
        if (w < 0) { perror("[VAULT] write"); return -1; }
        written += w;
    }
    return (ssize_t)written;
}

int vault_init() {
    if (mkdir(VAULT_DIR, 0755) < 0 && errno != EEXIST)
        return -1;
    return 0;
}

int vault_get_latest_version(const char *filename) {
    int version = 0;
    char path[600];
    while (1) {
        snprintf(path, sizeof(path), "%s/%s.v%d", VAULT_DIR, filename, version + 1);
        if (access(path, F_OK) != 0) break;
        version++;
    }
    return version;
}

int vault_push(const char *filename, const char *data, long size,
               const char *username, int base_version,
               int *new_version, char *conflict_diff, int diff_buf_len) {
    (void)conflict_diff;
    (void)diff_buf_len;

    pthread_rwlock_wrlock(&vault_rwlock);

    int current = vault_get_latest_version(filename);

    if (base_version > 0 && current != base_version) {
        pthread_rwlock_unlock(&vault_rwlock);
        return -2;
    }

    int next = current + 1;
    char path[600];
    snprintf(path, sizeof(path), "%s/%s.v%d", VAULT_DIR, filename, next);

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { pthread_rwlock_unlock(&vault_rwlock); return -1; }

    if (apply_fcntl_lock(fd, F_WRLCK) < 0) {
        close(fd);
        pthread_rwlock_unlock(&vault_rwlock);
        return -1;
    }

    write_full(fd, data, size);
    apply_fcntl_lock(fd, F_UNLCK);
    close(fd);

    char meta_path[600];
    snprintf(meta_path, sizeof(meta_path), "%s/%s.meta", VAULT_DIR, filename);
    int mfd = open(meta_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (mfd >= 0) {
        apply_fcntl_lock(mfd, F_WRLCK);
        char meta_line[256];
        int ml = snprintf(meta_line, sizeof(meta_line), "v%d|%s|%ld\n", next, username, (long)time(NULL));
        write_full(mfd, meta_line, ml);
        apply_fcntl_lock(mfd, F_UNLCK);
        close(mfd);
    }

    *new_version = next;
    pthread_rwlock_unlock(&vault_rwlock);
    return 0;
}

int vault_pull(const char *filename, int version,
               char **data, long *size, int *actual_version) {

    pthread_rwlock_rdlock(&vault_rwlock);

    int current = vault_get_latest_version(filename);
    if (current == 0) { pthread_rwlock_unlock(&vault_rwlock); return -1; }

    int target = (version <= 0 || version > current) ? current : version;

    char path[600];
    snprintf(path, sizeof(path), "%s/%s.v%d", VAULT_DIR, filename, target);

    int fd = open(path, O_RDONLY);
    if (fd < 0) { pthread_rwlock_unlock(&vault_rwlock); return -1; }

    if (apply_fcntl_lock(fd, F_RDLCK) < 0) {
        close(fd);
        pthread_rwlock_unlock(&vault_rwlock);
        return -1;
    }

    struct stat st;
    fstat(fd, &st);
    *size = st.st_size;
    *data = malloc(*size + 1);
    if (!*data) {
        apply_fcntl_lock(fd, F_UNLCK);
        close(fd);
        pthread_rwlock_unlock(&vault_rwlock);
        return -1;
    }

    long rd = 0;
    ssize_t n;
    while (rd < *size && (n = read(fd, *data + rd, *size - rd)) > 0)
        rd += n;

    apply_fcntl_lock(fd, F_UNLCK);
    close(fd);
    *actual_version = target;
    pthread_rwlock_unlock(&vault_rwlock);
    return 0;
}

int vault_list(char *output, int max_len) {
    pthread_rwlock_rdlock(&vault_rwlock);

    DIR *dir = opendir(VAULT_DIR);
    if (!dir) { pthread_rwlock_unlock(&vault_rwlock); return -1; }

    int written = 0;
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (!strstr(entry->d_name, ".meta")) continue;

        char name[MAX_FILENAME];
        strncpy(name, entry->d_name, MAX_FILENAME - 1);
        char *dot = strstr(name, ".meta");
        if (dot) *dot = '\0';

        int ver = vault_get_latest_version(name);
        written += snprintf(output + written, max_len - written,
            "  %-30s  versions: %-3d\n", name, ver);
        if (written >= max_len - 1) break;
    }

    closedir(dir);
    pthread_rwlock_unlock(&vault_rwlock);
    return written;
}

int vault_delete(const char *filename) {
    pthread_rwlock_wrlock(&vault_rwlock);
    int current = vault_get_latest_version(filename);
    if (current == 0) {
        pthread_rwlock_unlock(&vault_rwlock);
        return -1;
    }
    for (int v = 1; v <= current; v++) {
        char path[600];
        snprintf(path, sizeof(path), "%s/%s.v%d", VAULT_DIR, filename, v);
        unlink(path);
    }
    char meta_path[600];
    snprintf(meta_path, sizeof(meta_path), "%s/%s.meta", VAULT_DIR, filename);
    unlink(meta_path);
    pthread_rwlock_unlock(&vault_rwlock);
    return 0;
}
