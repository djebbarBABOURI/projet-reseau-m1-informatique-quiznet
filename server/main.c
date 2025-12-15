#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <strings.h>
#include <signal.h>

#include "sqlite/sql.h" 
#include "db_utils.h"
#include "handlers.h"
#include "sha256.h"
#include "server.h"
#include "udp.h"
#include "sqlite/sqlite3.h"
#include "utils.h"
#include "clients.h"

// const unsigned int BACKLOG = 7;
const unsigned int BLOC_ALLOC = 1024;
#define MAX_NB_CLIENT 1024
#define SIZE_LOGIN 50
#define ERR_LOGIN 1

typedef struct
{
  struct sockaddr_in addr;
  int fd;
  char login[SIZE_LOGIN];
  pthread_t thread;
  unsigned initDone;
} clientInfo;

unsigned nbClients = 0;
clientInfo clients[MAX_NB_CLIENT];
void broadcast_server_ready_udp(int udp_fd, int port);
// void broadcast_server_ready_udp(int udp_fd, int port) {
//     struct sockaddr_in broadcast_addr;
//     int broadcast_enable = 1;

//     // Activer le broadcast sur la socket UDP
//     if (setsockopt(udp_fd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
//         perror("setsockopt SO_BROADCAST");
//         return;
//     }

//     memset(&broadcast_addr, 0, sizeof(broadcast_addr));
//     broadcast_addr.sin_family = AF_INET;
//     broadcast_addr.sin_port = htons(port);
//     broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); // broadcast sur le réseau local

//     const char *msg = "POST server/ready\n{\"action\":\"server/ready\",\"message\":\"Le serveur est prêt !\"}";

//     if (sendto(udp_fd, msg, strlen(msg), 0,
//                (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
//         perror("Erreur broadcast server/ready UDP");
//     } else {
//         printf("Message 'server/ready' broadcasté sur le réseau local (port %d)\n", port);
//     }
// }

// void broadcast_server_ready_udp(int udp_fd) {
//     struct sockaddr_in broadcast_addr;
//     int broadcast_enable = 1;

//     // Activer le broadcast sur la socket UDP
//     if (setsockopt(udp_fd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
//         perror("setsockopt SO_BROADCAST");
//         return;
//     }

//     memset(&broadcast_addr, 0, sizeof(broadcast_addr));
//     broadcast_addr.sin_family = AF_INET;
//     broadcast_addr.sin_port = htons(6666); // port fixe pour le broadcast
//     broadcast_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); // broadcast sur le réseau local

//     const char *msg = "POST server/ready\n{\"action\":\"server/ready\",\"message\":\"Le serveur est prêt !\"}";

//     if (sendto(udp_fd, msg, strlen(msg), 0,
//                (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
//         perror("Erreur broadcast server/ready UDP");
//     } else {
//         printf("Message 'server/ready' broadcasté sur le réseau local (port 6666)\n");
//     }
// }

void broadcast_to_all_clients(const char *message);
void broadcast_server_shutdown(void) {
    // Message JSON que le client Python va intercepter
    const char *msg = "POST server/shutdown\n{\"action\":\"server/shutdown\",\"message\":\"Le serveur s'arrête.\"}";

    printf("Envoi du message d'arrêt à tous les clients...\n");

    // Si tu as déjà une fonction broadcast_to_all_clients, l'utiliser
    broadcast_to_all_clients(msg);

    // Fermer toutes les connexions clients proprement
    for (unsigned i = 0; i < nbClients; i++) {
        if (clients[i].fd > 0) {
            close(clients[i].fd);
            clients[i].fd = -1; // Marquer comme déconnecté
        }
    }

    printf("Tous les clients ont été notifiés et déconnectés.\n");
}
// Variable globale pour contrôler la boucle serveur
// Déclarations globales
volatile sig_atomic_t server_running = 1;
volatile sig_atomic_t shutdown_done = 0;

int server_fd = -1; // serveur global
int udp_fd = -1;    // socket UDP global

// Handler du signal SIGINT
void handle_sigint(int sig) {
    if (shutdown_done) return; // ignore si déjà en train d'arrêter
      shutdown_done = 1;

    printf("\nSignal SIGINT reçu, arrêt du serveur...\n");
    server_running = 0;          // demande d'arrêt du serveur
    // Fermer les sockets pour réveiller accept() et recvfrom()
    if (udp_fd >= 0) close(udp_fd);
    if (server_fd >= 0) shutdown(server_fd, SHUT_RDWR);
    broadcast_server_shutdown(); // notifier tous les clients
}

// void *communicationThread(void *data);
void addClient(clientInfo *client, struct sockaddr_in addr, int fd);

int main(int argc, char **argv)
{
  assert(argc > 1 && "Erreur: Aucun port spécifié. Utilisez un argument pour spécifier le port.");
  int port = atoi(argv[1]);
  printf("HELLO WORLD !\n");
  // int server_fd = initSocket(port, BACKLOG);
  // printf("Serveur initialisé avec succès sur le port %d. En attente de connexions...\n", port);
  // printf("Le descripteur de fichier du serveur est : %d\n", server_fd);

    // Initialiser le serveur
    server_fd = initSocket(port, BACKLOG);
    if (server_fd < 0) {
        perror("initSocket");
        exit(EXIT_FAILURE);
    }
  printf("Serveur initialisé avec succès sur le port %d. En attente de connexions...\n", port);
  printf("Le descripteur de fichier du serveur est : %d\n", server_fd);
    // Initialiser UDP
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_fd < 0) {
        perror("socket UDP");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

  // Diffuser que le serveur est prêt sur le réseau local
  // broadcast_server_ready_udp(udp_fd);
  // Installer le handler SIGINT
  signal(SIGINT, handle_sigint);
  
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);

  pthread_t udpThread;
  if (pthread_create(&udpThread, NULL, udpDiscoveryThread, &port) != 0) {
      perror("pthread_create UDP");
      exit(EXIT_FAILURE);
  }

  // initialisation SQLite
  sqlite3 *db;
  if (sqlite3_open("sqlite/quiznet.db", &db) != SQLITE_OK) {
      fprintf(stderr, "Erreur ouverture base : %s\n", sqlite3_errmsg(db));
      close(server_fd);
      return 1;
  }
  if (ensure_players_table(db) != SQLITE_OK) {
      fprintf(stderr, "Impossible d'initialiser la table players\n");
      sqlite3_close(db);
      close(server_fd);
      return 1;
  }

  create_tables(db);
  // populate_db(db);

  // Boucle principale avec contrôle server_running
  while (server_running)
  {
      int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
      if (client_fd == -1)
      {
        if (errno == EINTR) continue; // interrupted by signal, reprendre
        if (!server_running && (errno == EBADF || errno == EINVAL)) break;
        perror("Erreur accept()");
        continue;
      }

      char client_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
      int client_port = ntohs(client_addr.sin_port);

      printf("Connexion acceptée : %s:%d\n", client_ip, client_port);
      
      ClientsInfo client = { client_ip, client_port };
      isert_data(db, client);

      clientThreadArgs *args = malloc(sizeof(clientThreadArgs));
      if (!args) {
          fprintf(stderr, "Erreur allocation mémoire pour client\n");
          close(client_fd);
          continue;
      }
      args->fd = client_fd;
      args->db = db;
      args->addr = client_addr;

      pthread_t thread_id;
      if (pthread_create(&thread_id, NULL, communicationThread, args) != 0) {
          perror("pthread_create client");
          close(client_fd);
          free(args);
          continue;
      }
      pthread_detach(thread_id);
  }

  // Fermeture propre du serveur
  printf("Fermeture du serveur...\n");
  close(server_fd);
  sqlite3_close(db);

  // Arrêt du thread UDP
  pthread_cancel(udpThread);
  pthread_join(udpThread, NULL);

  return 0;
}
