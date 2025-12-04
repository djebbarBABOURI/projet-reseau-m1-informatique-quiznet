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
#include "sqlite/sql.h" 
#include "db_utils.h"
#include "handlers.h"
#include "sha256.h"
#include "server.h"
#include "udp.h"
#include "sqlite/sqlite3.h"
#include "utils.h"

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



// void *communicationThread(void *data);
void addClient(clientInfo *client, struct sockaddr_in addr, int fd);

int main(int argc, char **argv)
{
  assert(argc > 1 && "Erreur: Aucun port spécifié. Utilisez un argument pour spécifier le port.");
  int port = atoi(argv[1]);
  printf("HELLO WORLD !\n");
  int server_fd = initSocket(port, BACKLOG);
  printf("Serveur initialisé avec succès sur le port %d. En attente de connexions...\n", port);
  printf("Le descripteur de fichier du serveur est : %d\n", server_fd);
  
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
      return 1;
  }
  if (ensure_players_table(db) != SQLITE_OK) {
      fprintf(stderr, "Impossible d'initialiser la table players\n");
      return 1;
  }

  create_tables(db);
  // populate_db(db);

  while (1)
  {
      int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
      if (client_fd == -1)
      {
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

  close(server_fd);
  return 0;
}
