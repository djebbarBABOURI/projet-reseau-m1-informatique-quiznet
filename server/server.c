#include "server.h"
#include "handlers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "sqlite/sqlite3.h"
#include "db_utils.h"
#include "utils.h"

// Fonction pour initialiser le socket du serveur
int initSocket(int port, int backlog) {
    int server_fd;
    struct sockaddr_in server_addr;

    // Création du socket TCP
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse du serveur
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);


    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, backlog) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Serveur en écoute sur le port %d...\n", port);

    return server_fd;
}

void *communicationThread(void *data)
{
    clientThreadArgs *args = (clientThreadArgs *)data;
    int client_fd = args->fd;
    sqlite3 *db = args->db;
    char buffer[BUFFER_SIZE];
    size_t buffered = 0;

    while (1) {
        ssize_t bytes = recv(client_fd, buffer + buffered, BUFFER_SIZE - 1 - buffered, 0);
        if (bytes <= 0) {
            break;
        }
        buffered += (size_t)bytes;
        buffer[buffered] = '\0';

        if (!request_is_complete(buffer)) {
            continue;
        }

        handle_client_request(buffer, client_fd, db);
        buffered = 0;
        memset(buffer, 0, sizeof(buffer));
    }

    close(client_fd);
    free(args);
    pthread_exit(NULL);
}
