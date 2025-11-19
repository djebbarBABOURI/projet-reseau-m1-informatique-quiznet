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

typedef struct
{
  int fd;
  sqlite3 *db;
  struct sockaddr_in addr;
} clientThreadArgs;

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
static void handle_client_request(const char *request, int client_fd, sqlite3 *db);
static bool request_is_complete(const char *buffer);
static bool extract_json_string(const char *json, const char *key, char *out, size_t outSize);
static void hash_password(const char *password, char output[65]);
static int ensure_players_table(sqlite3 *db);
static int register_player(sqlite3 *db, const char *pseudo, const char *password_hash);
static int verify_player(sqlite3 *db, const char *pseudo, const char *password_hash);
static void send_json_response(int fd, const char *action, const char *status, const char *message);
static void send_register_success(int fd);
static void send_login_success(int fd);
static void send_error_response(int fd, const char *action, const char *status, const char *message);

// ==== Implémentation SHA-256 (public domain) ====
typedef struct {
    unsigned int state[8];
    unsigned long long bitcount;
    unsigned char buffer[64];
} sha256_ctx;

static const unsigned int sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static unsigned int rotr(unsigned int value, unsigned int count) {
    return (value >> count) | (value << (32 - count));
}

static void sha256_transform(sha256_ctx *ctx, const unsigned char data[]) {
    unsigned int m[64];
    unsigned int a,b,c,d,e,f,g,h;

    for (unsigned int i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = (data[j] << 24) | (data[j + 1] << 16) | (data[j + 2] << 8) | (data[j + 3]);
    }

    for (unsigned int i = 16; i < 64; ++i) {
        unsigned int s0 = rotr(m[i - 15], 7) ^ rotr(m[i - 15], 18) ^ (m[i - 15] >> 3);
        unsigned int s1 = rotr(m[i - 2], 17) ^ rotr(m[i - 2], 19) ^ (m[i - 2] >> 10);
        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (unsigned int i = 0; i < 64; ++i) {
        unsigned int s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        unsigned int ch = (e & f) ^ ((~e) & g);
        unsigned int temp1 = h + s1 + ch + sha256_k[i] + m[i];
        unsigned int s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        unsigned int maj = (a & b) ^ (a & c) ^ (b & c);
        unsigned int temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(sha256_ctx *ctx) {
    ctx->bitcount = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(sha256_ctx *ctx, const unsigned char *data, size_t len) {
    unsigned int i;
    for (i = 0; i < len; ++i) {
        ctx->buffer[ctx->bitcount & 0x3F] = data[i];
        ctx->bitcount++;
        if ((ctx->bitcount & 0x3F) == 0) {
            sha256_transform(ctx, ctx->buffer);
        }
    }
}

static void sha256_final(sha256_ctx *ctx, unsigned char hash[32]) {
    unsigned int i = ctx->bitcount & 0x3F;
    ctx->buffer[i++] = 0x80;

    if (i > 56) {
        while (i < 64) ctx->buffer[i++] = 0x00;
        sha256_transform(ctx, ctx->buffer);
        i = 0;
    }

    while (i < 56) ctx->buffer[i++] = 0x00;

    unsigned long long bitcount_be = ctx->bitcount << 3;
    for (int j = 7; j >= 0; --j) {
        ctx->buffer[i++] = (unsigned char)((bitcount_be >> (j * 8)) & 0xFF);
    }

    sha256_transform(ctx, ctx->buffer);

    for (int j = 0; j < 8; ++j) {
        hash[j * 4] = (ctx->state[j] >> 24) & 0xFF;
        hash[j * 4 + 1] = (ctx->state[j] >> 16) & 0xFF;
        hash[j * 4 + 2] = (ctx->state[j] >> 8) & 0xFF;
        hash[j * 4 + 3] = ctx->state[j] & 0xFF;
    }
}

static void hash_password(const char *password, char output[65]) {
    unsigned char hash[32];
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, (const unsigned char *)password, strlen(password));
    sha256_final(&ctx, hash);

    for (int i = 0; i < 32; ++i) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[64] = '\0';
}


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
  populate_db(db);

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

static bool request_is_complete(const char *buffer)
{
    if (strlen(buffer) == 0) return false;
    if (strstr(buffer, "\n\n") != NULL) return true;

    const char *json_start = strchr(buffer, '{');
    if (!json_start) {
        return strchr(buffer, '\n') != NULL;
    }
    int braces = 0;
    for (const char *p = json_start; *p; ++p) {
        if (*p == '{') braces++;
        else if (*p == '}') {
            braces--;
            if (braces == 0) return true;
        }
    }
    return false;
}

static void handle_client_request(const char *request, int client_fd, sqlite3 *db)
{
    char method[8] = {0};
    char path[128] = {0};

    if (sscanf(request, "%7s %127s", method, path) != 2) {
        send_error_response(client_fd, "unknown", "400", "Bad request");
        return;
    }

    const char *json = strchr(request, '{');
    char pseudo[128] = {0};
    char password[256] = {0};

    if (strcasecmp(method, "POST") == 0 && strcmp(path, "player/register") == 0) {
        if (!json) {
            send_error_response(client_fd, "player/register", "400", "Missing JSON body");
            return;
        }
        if (!extract_json_string(json, "pseudo", pseudo, sizeof(pseudo)) ||
            !extract_json_string(json, "password", password, sizeof(password))) {
            send_error_response(client_fd, "player/register", "400", "Missing pseudo or password");
            return;
        }

        char password_hash[65];
        hash_password(password, password_hash);
        int rc = register_player(db, pseudo, password_hash);
        if (rc == 0) {
            send_register_success(client_fd);
        } else if (rc == 1) {
            send_error_response(client_fd, "player/register", "409", "pseudo already exists");
        } else {
            send_error_response(client_fd, "player/register", "520", "Unknown Error");
        }
        return;
    }

    if (strcasecmp(method, "POST") == 0 && strcmp(path, "player/login") == 0) {
        if (!json) {
            send_error_response(client_fd, "player/login", "400", "Missing JSON body");
            return;
        }
        if (!extract_json_string(json, "pseudo", pseudo, sizeof(pseudo)) ||
            !extract_json_string(json, "password", password, sizeof(password))) {
            send_error_response(client_fd, "player/login", "400", "Missing pseudo or password");
            return;
        }
        char password_hash[65];
        hash_password(password, password_hash);
        int rc = verify_player(db, pseudo, password_hash);
        if (rc == 0) {
            send_login_success(client_fd);
        } else if (rc == 1) {
            send_error_response(client_fd, "player/login", "401", "invalid credentials");
        } else {
            send_error_response(client_fd, "player/login", "520", "Unknown Error");
        }
        return;
    }

    send_error_response(client_fd, "unknown", "404", "endpoint not implemented");
}

static bool extract_json_string(const char *json, const char *key, char *out, size_t outSize)
{
    if (!json || !key || !out) return false;
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *key_pos = strstr(json, pattern);
    if (!key_pos) return false;
    const char *colon = strchr(key_pos + strlen(pattern), ':');
    if (!colon) return false;
    colon++;
    while (*colon && (isspace((unsigned char)*colon) || *colon == '"')) {
        colon++;
    }
    const char *end = colon;
    while (*end && *end != '"' && *end != '\n' && *end != '\r') {
        end++;
    }
    size_t len = end - colon;
    if (len == 0 || len >= outSize) return false;
    strncpy(out, colon, len);
    out[len] = '\0';
    return true;
}

static int ensure_players_table(sqlite3 *db)
{
    const char *sql =
        "CREATE TABLE IF NOT EXISTS players ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "pseudo TEXT UNIQUE NOT NULL,"
        "password_hash TEXT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";
    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Erreur création table players: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
    return rc;
}

static int register_player(sqlite3 *db, const char *pseudo, const char *password_hash)
{
    const char *sql = "INSERT INTO players (pseudo, password_hash) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Erreur prepare register: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_text(stmt, 1, pseudo, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password_hash, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc == SQLITE_DONE) return 0;
    if (rc == SQLITE_CONSTRAINT) return 1;
    return -1;
}

static int verify_player(sqlite3 *db, const char *pseudo, const char *password_hash)
{
    const char *sql = "SELECT password_hash FROM players WHERE pseudo = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Erreur prepare login: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_text(stmt, 1, pseudo, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return 1; // not found
    }
    const unsigned char *stored = sqlite3_column_text(stmt, 0);
    int result = 1;
    if (stored && strcmp((const char *)stored, password_hash) == 0) {
        result = 0;
    }
    sqlite3_finalize(stmt);
    return result;
}

static void send_json_response(int fd, const char *action, const char *status, const char *message)
{
    char response[512];
    snprintf(response, sizeof(response),
             "{\"action\":\"%s\",\"statut\":\"%s\",\"message\":\"%s\"}\n",
             action, status, message);
    send(fd, response, strlen(response), 0);
}

static void send_register_success(int fd)
{
    send_json_response(fd, "player/register", "201", "player registered successfully");
}

static void send_login_success(int fd)
{
    send_json_response(fd, "player/login", "200", "login successful");
}

static void send_error_response(int fd, const char *action, const char *status, const char *message)
{
    send_json_response(fd, action, status, message);
}
