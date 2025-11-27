#ifndef CLIENTS_H
#define CLIENTS_H

#define MAX_CLIENTS 100

typedef struct {
    int fd;
    int player_id;
} ClientInfo;

void add_authenticated_client(int fd, int player_id);
int get_player_id_by_fd(int fd);
void remove_client(int fd);
void print_authenticated_clients();

#endif
