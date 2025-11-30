#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "clients.h"
#include "sqlite/sqlite3.h"

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

// Récupère tous les file descriptors des joueurs dans une session donnée
// Retourne le nombre de FDs trouvés, et remplit le tableau fds avec les descripteurs
int get_session_fds(sqlite3 *db, int session_id, int *fds, int max_fds) {
    if (!db || !fds || max_fds <= 0) return 0;
    
    // Requête SQL pour obtenir tous les player_id d'une session
    const char *sql = "SELECT player_id FROM players_session WHERE session_id = ?";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return 0;
    }
    
    sqlite3_bind_int(stmt, 1, session_id);
    
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_fds) {
        int player_id = sqlite3_column_int(stmt, 0);
        
        // Trouver le FD correspondant à ce player_id
        for (int i = 0; i < clients_count; i++) {
            if (clients[i].player_id == player_id) {
                fds[count++] = clients[i].fd;
                break;
            }
        }
    }
    
    sqlite3_finalize(stmt);
    return count;
}

// Obtient le file descriptor d'un joueur à partir de son ID
int get_fd_by_player_id(int player_id) {
    for (int i = 0; i < clients_count; i++) {
        if (clients[i].player_id == player_id) {
            return clients[i].fd;
        }
    }
    return -1;
}
