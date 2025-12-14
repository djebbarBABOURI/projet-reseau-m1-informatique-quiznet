#ifndef HANDLERS_H
#define HANDLERS_H

#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    int *sessions;
    int count;
} SessionList;

bool request_is_complete(const char *buffer);
bool extract_json_string(const char *json, const char *key, char *out, size_t outSize);
void handle_client_request(const char *request, int client_fd, sqlite3 *db);
void handle_themes_list(int fd, sqlite3 *db);
void handle_sessions_list(int fd, sqlite3 *db);
void handle_session_start(int client_fd, sqlite3 *db);
void handle_question_answer(const char *json, int client_fd, sqlite3 *db);
void broadcast_to_session(sqlite3 *db, int session_id, const char *message);
void send_session_finished(int session_id, sqlite3 *db);
void send_session_finished_solo(int session_id, sqlite3 *db);
void send_session_finished_battle(int session_id, sqlite3 *db);
void handle_player_logout(int client_fd, sqlite3 *db);
void handle_player_logout(int client_fd, sqlite3 *db);
#endif
