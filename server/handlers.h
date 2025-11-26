#ifndef HANDLERS_H
#define HANDLERS_H

#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>

bool request_is_complete(const char *buffer);
bool extract_json_string(const char *json, const char *key, char *out, size_t outSize);
void handle_client_request(const char *request, int client_fd, sqlite3 *db);
void handle_themes_list(int fd, sqlite3 *db);
void handle_sessions_list(int fd, sqlite3 *db);

#endif
