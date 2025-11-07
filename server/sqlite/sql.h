#ifndef SQL_H
#define SQL_H

#include "sqlite3.h"

typedef struct {
    const char *ip_address;
    int port;
} ClientsInfo;

int isert_data(sqlite3 *db, ClientsInfo client);

#endif
