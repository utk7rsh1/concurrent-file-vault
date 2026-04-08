#pragma once
#include <time.h>

#define SERVER_PORT     8080
#define MAX_CLIENTS     20
#define MAX_FILENAME    256
#define MAX_FILESIZE    (10 * 1024 * 1024)
#define MAX_FILES       100
#define VAULT_DIR       "vault_storage"
#define FIFO_PATH       "/tmp/vault_conflict_pipe"
#define SHM_KEY         0x4321
#define MSG_KEY         0x8765
#define SEM_NAME        "/vault_shm_sem"

typedef enum { OWNER = 0, CONTRIBUTOR = 1, VIEWER = 2 } Role;

typedef enum {
    CMD_AUTH = 1,
    CMD_PUSH,
    CMD_PULL,
    CMD_LIST,
    CMD_STATUS,
    CMD_QUIT
} CmdType;

typedef struct {
    CmdType type;
    char    filename[MAX_FILENAME];
    char    username[64];
    char    password[64];
    long    filesize;
    int     base_version;
} Request;

typedef struct {
    int  status;
    char message[512];
    long filesize;
    int  version;
} Response;

typedef struct {
    char   filename[MAX_FILENAME];
    int    latest_version;
    time_t last_modified;
    char   last_user[64];
} FileEntry;

typedef struct {
    int       file_count;
    FileEntry files[MAX_FILES];
} VersionManifest;

typedef struct {
    long   mtype;
    char   filename[MAX_FILENAME];
    int    version;
    char   username[64];
    time_t timestamp;
} IndexMsg;
