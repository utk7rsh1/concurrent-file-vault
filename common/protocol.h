#pragma once
#include <time.h>

#define SERVER_PORT     8080
#define MAX_CLIENTS     20
#define MAX_FILENAME    256
#define MAX_FILESIZE    (10 * 1024 * 1024)

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
