#ifndef SERVER_H
#define SERVER_H

#include "./sqlite/sqlite3.h"
#include <pthread.h>
#include <netinet/in.h>
#include <stddef.h>

#define BACKLOG 7
#define BUFFER_SIZE 1024

typedef struct {
    int fd;
    sqlite3 *db;
    struct sockaddr_in addr;
} clientThreadArgs;

int initSocket(int port, int backlog);
void *communicationThread(void *data);

#endif