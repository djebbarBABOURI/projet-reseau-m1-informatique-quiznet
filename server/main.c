#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "sqlite/sql.h" 

const unsigned int BACKLOG = 7;
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

void *communicationThread(void *data);
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

  // initialisation SQLite
  sqlite3 *db;
  if (sqlite3_open("sqlite/quiznet.db", &db) != SQLITE_OK) {
      fprintf(stderr, "Erreur ouverture base : %s\n", sqlite3_errmsg(db));
      return 1;
  }

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
      close(client_fd);
  }

  close(server_fd);
  return 0;
}
