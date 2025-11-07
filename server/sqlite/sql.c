#include "sqlite3.h"
#include <stdio.h>

typedef struct {
    const char *ip_address;
    int port;
} ClientsInfo;

int callback(void *NotUsed, int argc, char **argv, char **azColName){
    return 0;
}

int isert_data(sqlite3 *db, ClientsInfo client){
    char *err_msg = 0;
    char sql[256];
    snprintf(sql, sizeof(sql),
             "INSERT INTO clients_info(ip_address, port) VALUES ('%s', %d);",
             client.ip_address, client.port);

    int rc = sqlite3_exec(db, sql, callback, 0, &err_msg);

    if(rc != SQLITE_OK){
        fprintf(stderr, "SQL error : %s\n", err_msg);
        sqlite3_free(err_msg);
        return 1;
    }
    printf("Insertion réussie : %s:%d\n", client.ip_address, client.port);
    return 0;
}

// int main(){
//     sqlite3 *db;
//     char *err_msg = 0;

//     int rc = sqlite3_open("quiznet.db", &db);

//     if(rc != SQLITE_OK){
//         fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
//         sqlite3_close(db);

//         return 1;
//     }

//     char *sql =
//     "CREATE TABLE IF NOT EXISTS clients_info ("
//     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
//     "ip_address TEXT NOT NULL, "
//     "port INTEGER NOT NULL, "
//     "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
//     ");";

//     rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
//     if(rc != SQLITE_OK){
//         fprintf(stderr, "SQL error : %s\n", err_msg);

//         sqlite3_free(err_msg);
//         sqlite3_close(db);

//         return 1;
//     }

//     ClientsInfo client = {"127.0.0.1", 7000};
//     isert_data(db, client);
//     sqlite3_close(db);

//     return 0;
// }
