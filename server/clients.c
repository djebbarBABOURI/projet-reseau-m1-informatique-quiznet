#include <stdio.h>
#include <string.h>
#include "clients.h"

static ClientInfo clients[MAX_CLIENTS];
static int clients_count = 0;

void add_authenticated_client(int fd, int player_id) {
    // Supprimer si déjà existant
    remove_client(fd);

    if (clients_count < MAX_CLIENTS) {
        clients[clients_count].fd = fd;
        clients[clients_count].player_id = player_id;
        clients_count++;
    }

}

void print_authenticated_clients() {
    printf("=== Clients authentifiés (%d) ===\n", clients_count);
    for (int i = 0; i < clients_count; i++) {
        printf("Client #%d: fd=%d, player_id=%d\n", i, clients[i].fd, clients[i].player_id);
    }
    printf("================================\n");
}

int get_player_id_by_fd(int fd) {
    for (int i = 0; i < clients_count; i++) {
        if (clients[i].fd == fd)
            return clients[i].player_id;
    }
    return -1;
}

void remove_client(int fd) {
    for (int i = 0; i < clients_count; i++) {
        if (clients[i].fd == fd) {
            // décaler le reste du tableau
            memmove(&clients[i], &clients[i + 1], (clients_count - i - 1) * sizeof(ClientInfo));
            clients_count--;
            break;
        }
    }
}
