#include "udp.h"
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include "sqlite/sqlite3.h"
#include "handlers.h"
#include "db_utils.h"
#include "utils.h"

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

