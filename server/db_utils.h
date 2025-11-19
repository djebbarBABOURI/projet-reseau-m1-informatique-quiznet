#ifndef DB_UTILS_H
#define DB_UTILS_H

#include <sqlite3.h>

typedef struct {
    const char* question;
    const char* type;
    const char* options;
    const char* answer;
    const char* difficulty;
} Question;

void create_tables(sqlite3 *db);
int insert_theme(sqlite3 *db, const char *name);
int insert_question(sqlite3 *db, int theme_id, const char *difficulty,
                    const char *type, const char *enonce,
                    const char *answers_json, const char *correct_answer_json);
void populate_db(sqlite3 *db);

#endif