#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdbool.h>
#include "sqlite/sqlite3.h"
#include <stddef.h>

void send_json_response(int fd, const char *action, const char *status, const char *message);
void send_register_success(int fd);
void send_login_success(int fd);
void send_error_response(int fd, const char *action, const char *status, const char *message);

bool extract_json_int(const char *json, const char *key, int *out);
bool extract_json_int_array(const char *json, const char *key, int **out, size_t *count);

bool append_str(char **buffer, size_t *length, size_t *capacity, const char *text);
bool append_char(char **buffer, size_t *length, size_t *capacity, char c);
bool append_json_string(char **buffer, size_t *length, size_t *capacity, const unsigned char *text);

bool ensure_capacity(char **buffer, size_t *capacity, size_t required);
bool extract_json_raw(const char *json, const char *key, char *out, size_t outSize);

int store_player_answer(sqlite3 *db, int players_session_id, int question_id, const char *player_answer, double response_time);
char *trim(char *str);  
bool extract_json_double(const char *json, const char *key, double *out_value);

#endif
