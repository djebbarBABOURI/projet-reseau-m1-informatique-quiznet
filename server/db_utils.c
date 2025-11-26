#include "questions.h"
#include "db_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "sqlite/sqlite3.h"
#include "handlers.h"
#include "server.h"
#include <sys/socket.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include "utils.h"

void create_tables(sqlite3 *db) {
    const char *sql_statements[] = {
        "CREATE TABLE IF NOT EXISTS players ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "pseudo TEXT UNIQUE NOT NULL,"
        "password_hash TEXT NOT NULL,"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");",

        "CREATE TABLE IF NOT EXISTS themes ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name TEXT"
        ");",

        "CREATE TABLE IF NOT EXISTS questions ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    theme_id INTEGER REFERENCES themes(id),"
        "    difficulty TEXT CHECK(difficulty IN ('facile','moyen','difficile')),"
        "    type TEXT CHECK(type IN ('qcm','boolean','text')),"
        "    enonce TEXT,"
        "    answers TEXT,"
        "    correct_answer TEXT"
        ");",

        "CREATE TABLE IF NOT EXISTS sessions ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name TEXT,"
        "    difficulty TEXT,"
        "    nb_questions INTEGER,"
        "    time_limit INTEGER,"
        "    mode TEXT CHECK(mode IN ('solo','battle')),"
        "    max_players INTEGER,"
        "    status TEXT CHECK(status IN ('waiting','playing','finished')),"
        "    created_by INTEGER REFERENCES players(id)"
        ");",

        "CREATE TABLE IF NOT EXISTS themes_session ("
        "    session_id INTEGER REFERENCES sessions(id),"
        "    theme_id INTEGER REFERENCES themes(id),"
        "    PRIMARY KEY(session_id, theme_id)"
        ");",

        "CREATE TABLE IF NOT EXISTS players_session ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    player_id INTEGER REFERENCES players(id),"
        "    session_id INTEGER REFERENCES sessions(id),"
        "    lives INTEGER,"
        "    current_score INTEGER,"
        "    is_creator INTEGER,"
        "    eliminated_at INTEGER"
        ");",

        "CREATE TABLE IF NOT EXISTS jokers ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    players_session_id INTEGER REFERENCES players_session(id),"
        "    fifty INTEGER,"
        "    skip INTEGER"
        ");",

        "CREATE TABLE IF NOT EXISTS session_questions ("
        "    session_id INTEGER REFERENCES sessions(id),"
        "    question_id INTEGER REFERENCES questions(id),"
        "    num_question INTEGER,"
        "    PRIMARY KEY(session_id, num_question)"
        ");",

        "CREATE TABLE IF NOT EXISTS answers ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    players_session_id INTEGER REFERENCES players_session(id),"
        "    question_id INTEGER REFERENCES questions(id),"
        "    answer TEXT,"
        "    response_time REAL,"
        "    correct INTEGER,"
        "    points INTEGER"
        ");",

        "CREATE TABLE IF NOT EXISTS ranking ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    session_id INTEGER REFERENCES sessions(id),"
        "    player_id INTEGER REFERENCES players(id),"
        "    rank INTEGER,"
        "    score INTEGER,"
        "    correct_answers INTEGER,"
        "    eliminated_at INTEGER"
        ");"
    };

    size_t count = sizeof(sql_statements) / sizeof(sql_statements[0]);
    for (size_t i = 0; i < count; i++) {
        char *errmsg = 0;
        if (sqlite3_exec(db, sql_statements[i], 0, 0, &errmsg) != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", errmsg);
            sqlite3_free(errmsg);
        }
    }
}


// Noms des thèmes dans le même ordre
const char *themes_names[20] = {
    "Géographie",
    "Histoire",
    "Sciences",
    "Mathématiques",
    "Informatique",
    "Littérature",
    "Cinéma",
    "Musique",
    "Sport",
    "Art",
    "Cuisine",
    "Animaux",
    "Astronomie",
    "Technologie",
    "Philosophie",
    "Économie",
    "Politique",
    "Langues",
    "Religion",
    "Culture générale"
};


void populate_db(sqlite3 *db) {
    for (int t = 0; t < 20; t++) {
        if (insert_theme(db, themes_names[t]) != SQLITE_OK) continue;
        int theme_id = (int)sqlite3_last_insert_rowid(db);

        for (int q = 0; q < 10; q++) {
            Question *qn = &all_questions[t][q];

            if (insert_question(db,
                                theme_id,
                                qn->difficulty,
                                qn->type,
                                qn->question,
                                qn->options,
                                qn->answer) != SQLITE_OK) {
                fprintf(stderr, "Erreur insertion question %d thème %d (type='%s')\n", q, t, qn->type);
            }
        }
    }
}

// Fonction pour ajouter un thème
int insert_theme(sqlite3 *db, const char *name) {
    char sql[256];
    snprintf(sql, sizeof(sql),
             "INSERT INTO themes(name) VALUES('%s');", name);

    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error (insert_theme): %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }
    return SQLITE_OK;
}

// Fonction pour ajouter une question
int insert_question(sqlite3 *db, int theme_id, const char *difficulty,
                    const char *type, const char *enonce,
                    const char *answers_json, const char *correct_answer_json) {
    char sql[4096];
    snprintf(sql, sizeof(sql),
             "INSERT INTO questions(theme_id, difficulty, type, enonce, answers, correct_answer) "
             "VALUES(%d, '%s', '%s', '%s', '%s', '%s');",
             theme_id,
             difficulty,
             type,
             enonce,
             answers_json,
             correct_answer_json);

    char *err_msg = NULL;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error (insert_question): %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }
    return SQLITE_OK;
}

//
int ensure_players_table(sqlite3 *db)
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

//
int register_player(sqlite3 *db, const char *pseudo, const char *password_hash)
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

//
int verify_player(sqlite3 *db, const char *pseudo, const char *password_hash)
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

//
void hash_password(const char *password, char output[65]) {
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


// Créer les jokers pour un joueur dans une session
//
int create_jokers_for_player(sqlite3 *db, int players_session_id)
{
    const char *sql = "INSERT INTO jokers (players_session_id, fifty, skip) VALUES (?, 1, 1);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Erreur prepare create_jokers: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int(stmt, 1, players_session_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

// Gérer la création de session
//
void handle_session_create(const char *json, int fd, sqlite3 *db)
{
    char name[256] = {0};
    char difficulty[32] = {0};
    char mode[32] = {0};
    char pseudo[128] = {0};
    int nb_questions = 0;
    int time_limit = 0;
    int max_players = 0;
    int *theme_ids = NULL;
    size_t theme_count = 0;
    
    // Extraire les données JSON
    if (!extract_json_string(json, "name", name, sizeof(name)) ||
        !extract_json_string(json, "difficulty", difficulty, sizeof(difficulty)) ||
        !extract_json_string(json, "mode", mode, sizeof(mode)) ||
        !extract_json_int(json, "nbQuestions", &nb_questions) ||
        !extract_json_int(json, "timeLimit", &time_limit) ||
        !extract_json_int(json, "maxPlayers", &max_players) ||
        !extract_json_int_array(json, "themeIds", &theme_ids, &theme_count)) {
        if (theme_ids) free(theme_ids);
        send_error_response(fd, "session/create", "400", "Missing or invalid required fields");
        return;
    }
    
    // Pour l'instant, on utilise le pseudo du dernier login
    // TODO: Implémenter un système de session/token pour identifier le joueur
    // Pour l'instant, on va demander le pseudo dans la requête ou utiliser une valeur par défaut
    // Pour simplifier, on va chercher le pseudo dans la requête, sinon on utilise "anonymous"
    if (!extract_json_string(json, "pseudo", pseudo, sizeof(pseudo))) {
        // Si pas de pseudo, on ne peut pas créer la session
        if (theme_ids) free(theme_ids);
        send_error_response(fd, "session/create", "400", "Missing pseudo - user must be authenticated");
        return;
    }
    
    // Validation des valeurs
    if (nb_questions < 10 || nb_questions > 50) {
        if (theme_ids) free(theme_ids);
        send_error_response(fd, "session/create", "400", "nbQuestions must be between 10 and 50");
        return;
    }
    if (time_limit < 10 || time_limit > 60) {
        if (theme_ids) free(theme_ids);
        send_error_response(fd, "session/create", "400", "timeLimit must be between 10 and 60");
        return;
    }
    if (max_players < 2) {
        if (theme_ids) free(theme_ids);
        send_error_response(fd, "session/create", "400", "maxPlayers must be at least 2");
        return;
    }
    if (theme_count == 0) {
        if (theme_ids) free(theme_ids);
        send_error_response(fd, "session/create", "400", "At least one theme must be selected");
        return;
    }
    
    // Obtenir l'ID du joueur
    int player_id = get_player_id_by_pseudo(db, pseudo);
    if (player_id < 0) {
        if (theme_ids) free(theme_ids);
        send_error_response(fd, "session/create", "404", "Player not found");
        return;
    }
    
    // Créer la session
    int session_id = 0;
    if (create_session_in_db(db, name, difficulty, nb_questions, time_limit, mode, max_players, player_id, &session_id) != 0) {
        if (theme_ids) free(theme_ids);
        send_error_response(fd, "session/create", "500", "Failed to create session");
        return;
    }
    
    // Lier les thèmes
    if (link_themes_to_session(db, session_id, theme_ids, theme_count) != 0) {
        if (theme_ids) free(theme_ids);
        send_error_response(fd, "session/create", "500", "Failed to link themes");
        return;
    }
    
    // Ajouter le joueur à la session
    int players_session_id = 0;
    int lives = (strcmp(mode, "battle") == 0) ? 4 : 0; // 4 vies en mode battle, 0 en solo
    if (add_player_to_session(db, session_id, player_id, 1, lives, &players_session_id) != 0) {
        if (theme_ids) free(theme_ids);
        send_error_response(fd, "session/create", "500", "Failed to add player to session");
        return;
    }
    
    // Créer les jokers pour le joueur
    if (create_jokers_for_player(db, players_session_id) != 0) {
        // Non critique, on continue
    }
    
    // Envoyer la réponse de succès
    size_t capacity = 512;
    size_t length = 0;
    char *response = malloc(capacity);
    if (!response) {
        if (theme_ids) free(theme_ids);
        send_error_response(fd, "session/create", "500", "Memory error");
        return;
    }
    response[0] = '\0';
    
    char session_id_str[32];
    snprintf(session_id_str, sizeof(session_id_str), "%d", session_id);
    
    append_str(&response, &length, &capacity, "{\"action\":\"session/create\",\"statut\":\"201\",\"sessionId\":");
    append_str(&response, &length, &capacity, session_id_str);
    append_str(&response, &length, &capacity, ",\"isCreator\":true");
    
    // Ajouter les jokers
    append_str(&response, &length, &capacity, ",\"jokers\":{\"fifty\":1,\"skip\":1}");
    
    // Ajouter les vies si mode battle
    if (strcmp(mode, "battle") == 0) {
        append_str(&response, &length, &capacity, ",\"lives\":4");
    }
    
    append_str(&response, &length, &capacity, "}\n");
    
    send(fd, response, length, 0);
    free(response);
    if (theme_ids) free(theme_ids);
}


// Ajouter un joueur à une session
//
int add_player_to_session(sqlite3 *db, int session_id, int player_id, int is_creator, 
                                 int lives, int *players_session_id)
{
    const char *sql = "INSERT INTO players_session (player_id, session_id, lives, current_score, is_creator, eliminated_at) VALUES (?, ?, ?, 0, ?, NULL);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Erreur prepare add_player_to_session: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int(stmt, 1, player_id);
    sqlite3_bind_int(stmt, 2, session_id);
    sqlite3_bind_int(stmt, 3, lives);
    sqlite3_bind_int(stmt, 4, is_creator);
    
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Erreur add_player_to_session: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    
    *players_session_id = (int)sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return 0;
}

// Lier les thèmes à une session
//
int link_themes_to_session(sqlite3 *db, int session_id, int *theme_ids, size_t theme_count)
{
    const char *sql = "INSERT INTO themes_session (session_id, theme_id) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Erreur prepare link_themes: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    for (size_t i = 0; i < theme_count; i++) {
        sqlite3_reset(stmt);
        sqlite3_bind_int(stmt, 1, session_id);
        sqlite3_bind_int(stmt, 2, theme_ids[i]);
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Erreur link_themes: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return -1;
        }
    }
    
    sqlite3_finalize(stmt);
    return 0;
}

// Obtenir l'ID d'un joueur par son pseudo
static int get_player_id_by_pseudo(sqlite3 *db, const char *pseudo)
{
    const char *sql = "SELECT id FROM players WHERE pseudo = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    sqlite3_bind_text(stmt, 1, pseudo, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    int player_id = -1;
    if (rc == SQLITE_ROW) {
        player_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return player_id;
}

// Créer une session dans la base de données
static int create_session_in_db(sqlite3 *db, const char *name, const char *difficulty, 
                                 int nb_questions, int time_limit, const char *mode, 
                                 int max_players, int player_id, int *session_id)
{
    const char *sql = "INSERT INTO sessions (name, difficulty, nb_questions, time_limit, mode, max_players, status, created_by) VALUES (?, ?, ?, ?, ?, ?, 'waiting', ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Erreur prepare create_session: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, difficulty, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, nb_questions);
    sqlite3_bind_int(stmt, 4, time_limit);
    sqlite3_bind_text(stmt, 5, mode, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, max_players);
    sqlite3_bind_int(stmt, 7, player_id);
    
    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Erreur create_session: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    
    *session_id = (int)sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return 0;
}