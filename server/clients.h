#ifndef CLIENTS_H
#define CLIENTS_H

#include "sqlite/sqlite3.h"

#define MAX_CLIENTS 100

typedef struct {
    int fd;
    int player_id;
} ClientInfo;

void add_authenticated_client(int fd, int player_id);
int get_player_id_by_fd(int fd);
void remove_client(int fd);
void print_authenticated_clients();
int get_session_fds(sqlite3 *db, int session_id, int *fds, int max_fds);
int get_fd_by_player_id(int player_id);
void broadcast_to_all_clients(const char *message);
#endif
