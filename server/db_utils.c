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
#include "clients.h"

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
        "    created_by INTEGER REFERENCES players(id),"
        "    current_question INTEGER DEFAULT 1"
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
    printf("DEBUG populate_db: Début du peuplement de la base de données...\n");
    
    for (int t = 0; t < 20; t++) {
        if (insert_theme(db, themes_names[t]) != SQLITE_OK) {
            printf("DEBUG populate_db: Erreur insertion thème %d: %s\n", t, themes_names[t]);
            continue;
        }
        
        // Récupérer l'ID du thème (soit nouvellement créé, soit existant)
        int theme_id = -1;
        const char *sql_get_id = "SELECT id FROM themes WHERE name = ?";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db, sql_get_id, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, themes_names[t], -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                theme_id = sqlite3_column_int(stmt, 0);
            }
        }
        sqlite3_finalize(stmt);
        
        if (theme_id <= 0) {
            printf("DEBUG populate_db: Impossible de récupérer l'ID du thème %s\n", themes_names[t]);
            continue;
        }
        
        printf("DEBUG populate_db: Thème %d: %s (id=%d)\n", t, themes_names[t], theme_id);

        int questions_inserted = 0;
        for (int q = 0; q < 90; q++) {
            Question *qn = &all_questions[t][q];

            if (insert_question(db,
                                theme_id,
                                qn->difficulty,
                                qn->type,
                                qn->question,
                                qn->options,
                                qn->answer) == SQLITE_OK) {
                questions_inserted++;
            } else {
                fprintf(stderr, "Erreur insertion question %d thème %d (type='%s')\n", q, t, qn->type);
            }
        }
        printf("DEBUG populate_db: %d questions insérées pour le thème %s\n", questions_inserted, themes_names[t]);
    }
    
    printf("DEBUG populate_db: Peuplement terminé\n");
}

// Fonction pour ajouter un thème (ignore si déjà existant)
int insert_theme(sqlite3 *db, const char *name) {
    // Vérifier d'abord si le thème existe déjà
    const char *sql_check = "SELECT id FROM themes WHERE name = ?";
    sqlite3_stmt *stmt;
    int theme_id = -1;
    
    if (sqlite3_prepare_v2(db, sql_check, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            theme_id = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    
    // Si le thème existe déjà, retourner son ID
    if (theme_id > 0) {
        return SQLITE_OK;
    }
    
    // Sinon, l'insérer
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

    const char *sql =
        "INSERT INTO questions(theme_id, difficulty, type, enonce, answers, correct_answer) "
        "VALUES(?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "SQL prepare error: %s\n", sqlite3_errmsg(db));
        return SQLITE_ERROR;
    }

    sqlite3_bind_int(stmt, 1, theme_id);
    sqlite3_bind_text(stmt, 2, difficulty, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, enonce, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, answers_json, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, correct_answer_json, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "SQL step error: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
}
// int insert_question(sqlite3 *db, int theme_id, const char *difficulty,
//                     const char *type, const char *enonce,
//                     const char *answers_json, const char *correct_answer_json) {
//     char sql[4096];
//     snprintf(sql, sizeof(sql),
//              "INSERT INTO questions(theme_id, difficulty, type, enonce, answers, correct_answer) "
//              "VALUES(%d, '%s', '%s', '%s', '%s', '%s');",
//              theme_id,
//              difficulty,
//              type,
//              enonce,
//              answers_json,
//              correct_answer_json);

//     char *err_msg = NULL;
//     int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
//     if (rc != SQLITE_OK) {
//         fprintf(stderr, "SQL error (insert_question): %s\n", err_msg);
//         sqlite3_free(err_msg);
//         return rc;
//     }
//     return SQLITE_OK;
// }

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
    
    // Obtenir l'ID du joueur depuis le fd (joueur authentifié)
    // C'est plus sûr que d'utiliser le pseudo du JSON qui pourrait être incorrect
    int player_id = get_player_id_by_fd(fd);
    printf("DEBUG handle_session_create: pseudo='%s', player_id (from fd)=%d, fd=%d\n", pseudo, player_id, fd);
    if (player_id <= 0) {
        if (theme_ids) free(theme_ids);
        send_error_response(fd, "session/create", "401", "Player not authenticated");
        return;
    }
    
    // Vérifier aussi le player_id depuis le pseudo pour comparaison (debug)
    int player_id_from_pseudo = get_player_id_by_pseudo(db, pseudo);
    printf("DEBUG handle_session_create: player_id_from_pseudo=%d\n", player_id_from_pseudo);
    if (player_id_from_pseudo > 0 && player_id_from_pseudo != player_id) {
        printf("DEBUG handle_session_create: ATTENTION - player_id mismatch! pseudo donne %d mais fd donne %d\n", player_id_from_pseudo, player_id);
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
// int get_player_id_by_pseudo(sqlite3 *db, const char *pseudo)
// {
//     const char *sql = "SELECT id FROM players WHERE pseudo = ?;";
//     sqlite3_stmt *stmt;
//     if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
//         return -1;
//     }
//     sqlite3_bind_text(stmt, 1, pseudo, -1, SQLITE_TRANSIENT);
//     int rc = sqlite3_step(stmt);
//     int player_id = -1;
//     if (rc == SQLITE_ROW) {
//         player_id = sqlite3_column_int(stmt, 0);
//     }
//     sqlite3_finalize(stmt);
//     return player_id;
// }

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


// Renvoie le nombre de joueurs dans une session
int get_nb_players(sqlite3 *db, int session_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*) FROM players_session WHERE session_id = ?";
    int nb = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, session_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            nb = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return nb;
}

// Renvoie le mode d'une session
bool get_session_mode(sqlite3 *db, int session_id, char *mode, size_t mode_len) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT mode FROM sessions WHERE id = ?";
    bool found = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, session_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char *m = sqlite3_column_text(stmt, 0);
            strncpy(mode, (const char *)m, mode_len - 1);
            mode[mode_len - 1] = '\0';
            found = true;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

int get_player_id_by_pseudo(sqlite3 *db, const char *pseudo) {
    if (!db || !pseudo) return -1;

    const char *sql = "SELECT id FROM players WHERE pseudo = ?;";
    sqlite3_stmt *stmt = NULL;
    int player_id = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }

    if (sqlite3_bind_text(stmt, 1, pseudo, -1, SQLITE_STATIC) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return -1;
    }

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        player_id = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return player_id;
}

// Fonction principale pour rejoindre une session
void join_session(int fd, sqlite3 *db, int player_id, int session_id) {
    char mode[16];
    if (!get_session_mode(db, session_id, mode, sizeof(mode))) {
        send_error_response(fd, "session/join", "404", "session not found");
        return;
    }

    // Vérifier le nombre de joueurs
    sqlite3_stmt *stmt;
    const char *sql_max = "SELECT max_players FROM sessions WHERE id = ?";
    int max_players = 0;
    if (sqlite3_prepare_v2(db, sql_max, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, session_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            max_players = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);

    int nb_players = get_nb_players(db, session_id);
    if (nb_players >= max_players) {
        send_error_response(fd, "session/join", "403", "session is full");
        return;
    }

    // Vérifier si le joueur est le créateur de la session
    bool is_creator = is_session_creator(db, session_id, player_id);
    
    // Ajouter le joueur à players_session
    const char *sql_insert = "INSERT INTO players_session (player_id, session_id, lives, current_score, is_creator) VALUES (?, ?, ?, 0, ?)";
    if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL) == SQLITE_OK) {
        int lives = strcmp(mode, "battle") == 0 ? 4 : 0; // 4 vies en mode battle selon le protocole
        sqlite3_bind_int(stmt, 1, player_id);
        sqlite3_bind_int(stmt, 2, session_id);
        sqlite3_bind_int(stmt, 3, lives);
        sqlite3_bind_int(stmt, 4, is_creator ? 1 : 0);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);

    // Récupérer l'id du joueur_session
    int players_session_id = (int)sqlite3_last_insert_rowid(db);

    // Créer jokers (toujours créer les jokers, même en mode solo selon le protocole)
    const char *sql_jokers = "INSERT INTO jokers (players_session_id, fifty, skip) VALUES (?, 1, 1)";
    if (sqlite3_prepare_v2(db, sql_jokers, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, players_session_id);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);

    // Construire la réponse JSON
    int lives = strcmp(mode, "battle") == 0 ? 4 : 0;
    char response[1024];
    if (strcmp(mode, "battle") == 0) {
        snprintf(response, sizeof(response),
                 "{\"action\":\"session/join\",\"statut\":\"201\",\"message\":\"session joined\","
                 "\"sessionId\":%d,\"mode\":\"%s\",\"isCreator\":%s,\"players\":[],\"lives\":%d,\"jokers\":{\"fifty\":1,\"skip\":1}}",
                 session_id, mode, is_creator ? "true" : "false", lives);
    } else {
        snprintf(response, sizeof(response),
                 "{\"action\":\"session/join\",\"statut\":\"201\",\"message\":\"session joined\","
                 "\"sessionId\":%d,\"mode\":\"%s\",\"isCreator\":%s,\"players\":[],\"jokers\":{\"fifty\":1,\"skip\":1}}",
                 session_id, mode, is_creator ? "true" : "false");
    }

    send(fd, response, strlen(response), 0);

    // Envoyer POST session/player/joined à tous les autres joueurs
    // Récupérer le pseudo du joueur qui vient de rejoindre
    const char *sql_pseudo = "SELECT pseudo FROM players WHERE id = ?";
    sqlite3_stmt *stmt_pseudo;
    char player_pseudo[128] = {0};
    if (sqlite3_prepare_v2(db, sql_pseudo, -1, &stmt_pseudo, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt_pseudo, 1, player_id);
        if (sqlite3_step(stmt_pseudo) == SQLITE_ROW) {
            const unsigned char *p = sqlite3_column_text(stmt_pseudo, 0);
            if (p) {
                strncpy(player_pseudo, (const char *)p, sizeof(player_pseudo) - 1);
            }
        }
    }
    sqlite3_finalize(stmt_pseudo);
    
    // Compter le nouveau nombre de joueurs
    int new_nb_players = get_nb_players(db, session_id);
    
    // Envoyer le message à tous les autres joueurs de la session
    char joined_message[512];
    snprintf(joined_message, sizeof(joined_message),
        "POST session/player/joined\n{\"action\":\"session/player/joined\",\"pseudo\":\"%s\",\"nbPlayers\":%d}\n",
        player_pseudo, new_nb_players);
    
    // Récupérer tous les FDs de la session sauf celui qui vient de rejoindre
    int fds[100];
    int count = get_session_fds(db, session_id, fds, 100);
    for (int i = 0; i < count; i++) {
        if (fds[i] != fd) {  // Ne pas envoyer au joueur qui vient de rejoindre
            send(fds[i], joined_message, strlen(joined_message), 0);
        }
    }
    
    // Si maintenant il y a 2 joueurs, envoyer un message spécial au créateur
    if (new_nb_players >= 2) {
        // Récupérer le créateur de la session
        const char *sql_creator = "SELECT created_by FROM sessions WHERE id = ?";
        sqlite3_stmt *stmt_creator;
        int creator_id = -1;
        if (sqlite3_prepare_v2(db, sql_creator, -1, &stmt_creator, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt_creator, 1, session_id);
            if (sqlite3_step(stmt_creator) == SQLITE_ROW) {
                creator_id = sqlite3_column_int(stmt_creator, 0);
            }
        }
        sqlite3_finalize(stmt_creator);
        
        // Envoyer un message au créateur pour indiquer que la session est prête
        if (creator_id > 0) {
            int creator_fd = get_fd_by_player_id(creator_id);
            if (creator_fd > 0) {
                char ready_message[256];
                snprintf(ready_message, sizeof(ready_message),
                    "POST session/ready\n{\"action\":\"session/ready\",\"message\":\"Session prête à démarrer\",\"nbPlayers\":%d}\n",
                    new_nb_players);
                send(creator_fd, ready_message, strlen(ready_message), 0);
            }
        }
    }
}

int count_players_in_session(void* db, int session_id) {
    int count = 0;
    // Exemple pour SQLite
    sqlite3_stmt *stmt;
    const char *sql = "SELECT COUNT(*) FROM players_session WHERE session_id = ?;";

    if (sqlite3_prepare_v2((sqlite3*)db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, session_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);
    return count;
}

// Vérifie si un joueur est le créateur d'une session
bool is_session_creator(sqlite3 *db, int session_id, int player_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT created_by FROM sessions WHERE id = ?";
    bool is_creator = false;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, session_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int creator_id = sqlite3_column_int(stmt, 0);
            is_creator = (creator_id == player_id);
        }
    }
    sqlite3_finalize(stmt);
    return is_creator;
}

// Met à jour le statut d'une session
int update_session_status(sqlite3 *db, int session_id, const char *status) {
    const char *sql = "UPDATE sessions SET status = ? WHERE id = ?";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_text(stmt, 1, status, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, session_id);
    
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    return (rc == SQLITE_DONE) ? 0 : -1;
}

// Sélectionne aléatoirement les questions pour une session selon les thèmes et la difficulté
// Retourne le nombre de questions sélectionnées, ou -1 en cas d'erreur
int select_questions_for_session(sqlite3 *db, int session_id, int nb_questions) {
    // Récupérer les thèmes et la difficulté de la session
    const char *sql_info = "SELECT difficulty FROM sessions WHERE id = ?";
    sqlite3_stmt *stmt_info;
    char difficulty[32] = {0};
    
    if (sqlite3_prepare_v2(db, sql_info, -1, &stmt_info, NULL) != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_int(stmt_info, 1, session_id);
    if (sqlite3_step(stmt_info) != SQLITE_ROW) {
        sqlite3_finalize(stmt_info);
        return -1;
    }
    
    const unsigned char *diff = sqlite3_column_text(stmt_info, 0);
    if (diff) {
        strncpy(difficulty, (const char *)diff, sizeof(difficulty) - 1);
    }
    sqlite3_finalize(stmt_info);
    
    // Récupérer les thèmes de la session
    const char *sql_themes = "SELECT theme_id FROM themes_session WHERE session_id = ?";
    sqlite3_stmt *stmt_themes;
    int theme_ids[20];
    int theme_count = 0;
    
    if (sqlite3_prepare_v2(db, sql_themes, -1, &stmt_themes, NULL) != SQLITE_OK) {
        return -1;
    }
    
    sqlite3_bind_int(stmt_themes, 1, session_id);
    while (sqlite3_step(stmt_themes) == SQLITE_ROW && theme_count < 20) {
        theme_ids[theme_count++] = sqlite3_column_int(stmt_themes, 0);
    }
    sqlite3_finalize(stmt_themes);
    
    if (theme_count == 0) {
        return -1;
    }
    
    // Construire la clause IN pour les thèmes
    char theme_list[256] = {0};
    for (int i = 0; i < theme_count; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", theme_ids[i]);
        if (i > 0) strcat(theme_list, ",");
        strcat(theme_list, buf);
    }
    
    // D'abord, compter combien de questions sont disponibles
    char sql_count[512];
    snprintf(sql_count, sizeof(sql_count),
        "SELECT COUNT(*) FROM questions WHERE theme_id IN (%s) AND difficulty = ?",
        theme_list);
    
    sqlite3_stmt *stmt_count;
    int available_questions = 0;
    if (sqlite3_prepare_v2(db, sql_count, -1, &stmt_count, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt_count, 1, difficulty, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt_count) == SQLITE_ROW) {
            available_questions = sqlite3_column_int(stmt_count, 0);
        }
    }
    sqlite3_finalize(stmt_count);
    
    printf("DEBUG select_questions_for_session: difficulty='%s', theme_count=%d, available_questions=%d, nb_questions_demandedes=%d\n", 
           difficulty, theme_count, available_questions, nb_questions);
    
    if (available_questions == 0) {
        printf("DEBUG select_questions_for_session: Aucune question disponible pour ces critères\n");
        return -1;
    }
    
    // Sélectionner les questions correspondant aux critères
    char sql_select[512];
    snprintf(sql_select, sizeof(sql_select),
        "SELECT id FROM questions WHERE theme_id IN (%s) AND difficulty = ? ORDER BY RANDOM() LIMIT ?",
        theme_list);
    
    sqlite3_stmt *stmt_select;
    if (sqlite3_prepare_v2(db, sql_select, -1, &stmt_select, NULL) != SQLITE_OK) {
        printf("DEBUG select_questions_for_session: Erreur prepare SQL\n");
        return -1;
    }
    
    sqlite3_bind_text(stmt_select, 1, difficulty, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt_select, 2, nb_questions);
    
    // Insérer les questions sélectionnées dans session_questions
    const char *sql_insert = "INSERT INTO session_questions (session_id, question_id, num_question) VALUES (?, ?, ?)";
    sqlite3_stmt *stmt_insert;
    if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt_insert, NULL) != SQLITE_OK) {
        sqlite3_finalize(stmt_select);
        return -1;
    }
    
    int question_num = 1;
    int questions_selected = 0;
    
    while (sqlite3_step(stmt_select) == SQLITE_ROW && question_num <= nb_questions) {
        int question_id = sqlite3_column_int(stmt_select, 0);
        
        sqlite3_bind_int(stmt_insert, 1, session_id);
        sqlite3_bind_int(stmt_insert, 2, question_id);
        sqlite3_bind_int(stmt_insert, 3, question_num);
        
        if (sqlite3_step(stmt_insert) == SQLITE_DONE) {
            questions_selected++;
        } else {
            printf("DEBUG select_questions_for_session: Erreur insertion question %d (id=%d)\n", question_num, question_id);
        }
        
        sqlite3_reset(stmt_insert);
        question_num++;
    }
    
    sqlite3_finalize(stmt_select);
    sqlite3_finalize(stmt_insert);
    
    printf("DEBUG select_questions_for_session: questions_selected=%d sur %d demandées\n", questions_selected, nb_questions);
    
    return questions_selected;
}