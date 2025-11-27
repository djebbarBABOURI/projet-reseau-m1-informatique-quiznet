#ifndef DB_UTILS_H
#define DB_UTILS_H

#include "sqlite/sqlite3.h"
#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>
#include "sha256.h"

void create_tables(sqlite3 *db);
int insert_theme(sqlite3 *db, const char *name);
int insert_question(sqlite3 *db, int theme_id, const char *difficulty,
                    const char *type, const char *enonce,
                    const char *answers_json, const char *correct_answer_json);
void populate_db(sqlite3 *db);

int ensure_players_table(sqlite3 *db);
int register_player(sqlite3 *db, const char *pseudo, const char *password_hash);
int verify_player(sqlite3 *db, const char *pseudo, const char *password_hash);
void hash_password(const char *password, char output[65]);
int create_jokers_for_player(sqlite3 *db, int players_session_id);
void handle_session_create(const char *json, int fd, sqlite3 *db);
int add_player_to_session(sqlite3 *db, int session_id, int player_id, int is_creator, 
                                 int lives, int *players_session_id);
int link_themes_to_session(sqlite3 *db, int session_id, int *theme_ids, size_t theme_count);
static int create_session_in_db(sqlite3 *db, const char *name, const char *difficulty, 
                                 int nb_questions, int time_limit, const char *mode, 
                                 int max_players, int player_id, int *session_id);

int get_nb_players(sqlite3 *db, int session_id);

bool get_session_mode(sqlite3 *db, int session_id, char *mode, size_t mode_len);

void join_session(int fd, sqlite3 *db, int player_id, int session_id);
int get_player_id_by_pseudo(sqlite3 *db, const char *pseudo);
int count_players_in_session(void* db, int session_id);
#endif
