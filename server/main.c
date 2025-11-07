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

#define UDP_PORT 5555
#define BUFFER_SIZE 1024
#define SERVER_NAME "QuizNetServer"

void *udpDiscoveryThread(void *arg) {
    int tcpPort = *((int *)arg);
    int udpSock;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[BUFFER_SIZE];
    socklen_t len = sizeof(cliaddr);

    // Création du socket UDP
    if ((udpSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("UDP socket");
        pthread_exit(NULL);
    }

    int opt = 1;
    if (setsockopt(udpSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(udpSock);
        pthread_exit(NULL);
    }

    #ifdef SO_REUSEPORT
    if (setsockopt(udpSock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEPORT");
        close(udpSock);
        pthread_exit(NULL);
    }
    #endif

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(UDP_PORT);

    if (bind(udpSock, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("UDP bind");
        close(udpSock);
        pthread_exit(NULL);
    }

    printf("UDP Discovery running on port %d...\n", UDP_PORT);

    while (1) {
        int n = recvfrom(udpSock, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) continue;
        buffer[n] = '\0';
        buffer[strcspn(buffer, "\r\n")] = 0;
        printf("UDP message reçu : '%s' de %s:%d\n", buffer, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
        // Vérification du message
        if (strcmp(buffer, "looking for quiznet servers") == 0) {
            char response[BUFFER_SIZE];
            snprintf(response, BUFFER_SIZE, "hello i'm a quiznet server:%s:%d", SERVER_NAME, tcpPort);
            sendto(udpSock, response, strlen(response), 0, (struct sockaddr *)&cliaddr, len);
            printf("UDP response sent to client %s:%d\n",
                   inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
        }
    }

    close(udpSock);
    pthread_exit(NULL);
}


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
