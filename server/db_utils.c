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
        "    correct_answer TEXT,"
        "    explanation TEXT DEFAULT 'Aucune explication'"
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
        "    current_question INTEGER DEFAULT 0"
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

int get_correct_answer_positions(
    sqlite3 *db,
    int question_id,
    int *positions,
    int max_positions
) {
    if (!db || !positions || max_positions <= 0) return -1;

    sqlite3_stmt *stmt;
    char answers_raw[1024] = {0};
    char correct_raw[1024] = {0};

    /* 1. Récupérer answers et correct_answer */
    const char *sql =
        "SELECT answers, correct_answer FROM questions WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return -1;

    sqlite3_bind_int(stmt, 1, question_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *a = sqlite3_column_text(stmt, 0);
        const unsigned char *c = sqlite3_column_text(stmt, 1);

        if (a) strncpy(answers_raw, (const char *)a, sizeof(answers_raw) - 1);
        if (c) strncpy(correct_raw, (const char *)c, sizeof(correct_raw) - 1);
    } else {
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);

    /* 2. Nettoyer answers */
    char answers_clean[1024] = {0};
    int j = 0;
    for (int i = 0; answers_raw[i]; i++) {
        if (answers_raw[i] != '[' &&
            answers_raw[i] != ']' &&
            answers_raw[i] != '"')
        {
            answers_clean[j++] = answers_raw[i];
        }
    }
    answers_clean[j] = '\0';

    /* 3. Nettoyer correct_answer */
    char correct_clean[1024] = {0};
    j = 0;
    for (int i = 0; correct_raw[i]; i++) {
        if (correct_raw[i] != '[' &&
            correct_raw[i] != ']' &&
            correct_raw[i] != '"')
        {
            correct_clean[j++] = correct_raw[i];
        }
    }
    correct_clean[j] = '\0';

    /* 4. Trouver les indices */
    int found = 0;

    char *answers_copy = strdup(answers_clean);
    char *correct_copy = strdup(correct_clean);

    char *correct_tok = strtok(correct_copy, ",");

    while (correct_tok && found < max_positions) {

        while (*correct_tok == ' ') correct_tok++;
        char *end = correct_tok + strlen(correct_tok) - 1;
        while (end > correct_tok && *end == ' ') *end-- = '\0';

        int index = 0;
        char *answers_tok = strtok(answers_copy, ",");

        while (answers_tok) {

            while (*answers_tok == ' ') answers_tok++;
            end = answers_tok + strlen(answers_tok) - 1;
            while (end > answers_tok && *end == ' ') *end-- = '\0';

            if (strcmp(answers_tok, correct_tok) == 0) {
                positions[found++] = index;
                break;
            }

            index++;
            answers_tok = strtok(NULL, ",");
        }

        /* reset answers tokenizer */
        strcpy(answers_copy, answers_clean);
        correct_tok = strtok(NULL, ",");
    }

    free(answers_copy);
    free(correct_copy);

    return found;
}


void trim_quotes_spaces(char *s) {
    if (!s) return;
    // début
    char *p = s;
    while (*p == ' ' || *p == '"') p++;
    memmove(s, p, strlen(p)+1);

    // fin
    int len = strlen(s);
    while(len > 0 && (s[len-1] == ' ' || s[len-1] == '"')) s[--len] = '\0';
}

// Fonction principale pour stocker la réponse d'un joueur
int store_player_answer(sqlite3 *db, int players_session_id, int question_id, const char *player_answer, double response_time) {
    if (!db || !player_answer) return -1;

    int correct = 0;
    int points = 0;

    // 1. Récupérer correct_answer, difficulty, type et answers (pour QCM)
    const char *sql_get = "SELECT correct_answer, difficulty, type, answers FROM questions WHERE id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql_get, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[ERROR] prepare SELECT: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    sqlite3_bind_int(stmt, 1, question_id);

    char correct_answer[1024] = {0};
    char difficulty[32] = {0};
    char type[32] = {0};
    char answers[1024] = {0};

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *c = sqlite3_column_text(stmt, 0);
        const unsigned char *d = sqlite3_column_text(stmt, 1);
        const unsigned char *t = sqlite3_column_text(stmt, 2);
        const unsigned char *a = sqlite3_column_text(stmt, 3);

        if (c) strncpy(correct_answer, (const char *)c, sizeof(correct_answer)-1);
        if (d) strncpy(difficulty, (const char *)d, sizeof(difficulty)-1);
        if (t) strncpy(type, (const char *)t, sizeof(type)-1);
        if (a) strncpy(answers, (const char *)a, sizeof(answers)-1);
    } else {
        sqlite3_finalize(stmt);
        fprintf(stderr, "[ERROR] Question non trouvée (id=%d)\n", question_id);
        return -1;
    }
    sqlite3_finalize(stmt);

    printf("[DEBUG] Raw data -> player_answer='%s', correct_answer='%s', type='%s', answers='%s'\n",
           player_answer, correct_answer, type, answers);

    // Nettoyer player_answer
    char player_clean[1024];
    strncpy(player_clean, player_answer, sizeof(player_clean)-1);
    player_clean[sizeof(player_clean)-1] = '\0';
    trim_quotes_spaces(player_clean);
    printf("[DEBUG] Cleaned player_answer='%s'\n", player_clean);

    // 2. Vérifier le type
   if (strcmp(type, "qcm") == 0) {
    int positions[10]; // max 10 bonnes réponses
    int nb_correct = get_correct_answer_positions(db, question_id, positions, 10);
    printf("[DEBUG] QCM nb_correct=%d, positions=", nb_correct);
    for(int i=0;i<nb_correct;i++) printf("%d ", positions[i]);
    printf("\n");

    // Nettoyer answers
    char answers_clean[1024];
    strncpy(answers_clean, answers, sizeof(answers_clean)-1);
    answers_clean[sizeof(answers_clean)-1] = '\0';
    trim_quotes_spaces(answers_clean);

    // Convertir player_answer en entier (indice)
    int player_idx = atoi(player_clean);
    printf("[DEBUG-QCM] Player selected index=%d\n", player_idx);

    // Vérifier si l'indice est dans les positions correctes
    correct = 0;
    for (int i = 0; i < nb_correct; i++) {
        if (player_idx == positions[i]) {
            correct = 1;
            break;
        }
    }
    printf("[DEBUG-QCM] Result correct=%d\n", correct);
}else if (strcmp(type, "boolean") == 0 || strcmp(type, "text") == 0) {
        // Nettoyer correct_answer
        char clean_correct[1024] = {0};
        int j = 0;
        for (int i = 0; correct_answer[i] != '\0' && j < sizeof(clean_correct)-1; i++) {
            if (correct_answer[i] != '[' && correct_answer[i] != ']' && correct_answer[i] != '"') {
                clean_correct[j++] = correct_answer[i];
            }
        }
        clean_correct[j] = '\0';
        trim_quotes_spaces(clean_correct);
        printf("[DEBUG] Cleaned correct_answer='%s'\n", clean_correct);

        // Comparer exactement
        correct = (strcmp(clean_correct, player_clean) == 0) ? 1 : 0;
        printf("[DEBUG] Result correct=%d\n", correct);
    }

    int time_limit = 0;

    /* Récupérer le time_limit de la session */
    const char *sql_time =
        "SELECT s.time_limit "
        "FROM sessions s "
        "JOIN players_session ps ON ps.session_id = s.id "
        "WHERE ps.id = ?;";

    if (sqlite3_prepare_v2(db, sql_time, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, players_session_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            time_limit = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);

    printf("[DEBUG] time_limit=%d, response_time=%.2f\n", time_limit, response_time);

    /* 3. Calculer les points avec bonus rapidité */
    if (correct) {
        int base_points = 0;
        int bonus = 0;

        if (strcmp(difficulty, "facile") == 0) {
            base_points = 5;
            bonus = 1;
        } else if (strcmp(difficulty, "moyen") == 0) {
            base_points = 10;
            bonus = 3;
        } else if (strcmp(difficulty, "difficile") == 0) {
            base_points = 15;
            bonus = 6;
        }

        points = base_points;

        /* Bonus si réponse avant la moitié du temps */
        if (time_limit > 0 && response_time <= (time_limit / 2.0)) {
            points += bonus;
            printf("[DEBUG] Bonus rapidité appliqué: +%d\n", bonus);
        }
    }

    printf("[DEBUG] points finaux=%d\n", points);

    // 4. Insérer dans la table
    const char *sql_insert =
        "INSERT INTO answers (players_session_id, question_id, answer, response_time, correct, points) "
        "VALUES (?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[ERROR] prepare INSERT: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, players_session_id);
    sqlite3_bind_int(stmt, 2, question_id);
    sqlite3_bind_text(stmt, 3, player_clean, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, response_time);
    sqlite3_bind_int(stmt, 5, correct);
    sqlite3_bind_int(stmt, 6, points);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "[ERROR] INSERT: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }

    sqlite3_finalize(stmt);
    /* 5. Mettre à jour le score du joueur */
    if (points > 0) {
        const char *sql_update_score =
            "UPDATE players_session "
            "SET current_score = COALESCE(current_score, 0) + ? "
            "WHERE id = ?;";

        if (sqlite3_prepare_v2(db, sql_update_score, -1, &stmt, NULL) != SQLITE_OK) {
            fprintf(stderr, "[ERROR] prepare UPDATE score: %s\n", sqlite3_errmsg(db));
            return -1;
        }

        sqlite3_bind_int(stmt, 1, points);
        sqlite3_bind_int(stmt, 2, players_session_id);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            fprintf(stderr, "[ERROR] UPDATE score: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return -1;
        }

        sqlite3_finalize(stmt);
        printf("[DEBUG] Score updated: +%d points for players_session_id=%d\n",
            points, players_session_id);
    }

    printf("[DEBUG] Answer stored successfully: correct=%d, points=%d\n", correct, points);
    return 0;
}

// int store_player_answer(sqlite3 *db, int players_session_id, int question_id, const char *player_answer, double response_time) {
//     if (!db || !player_answer) return -1;

//     int correct = 0;
//     int points = 0;

//     // 1. Récupérer la réponse correcte et la difficulté
//     const char *sql_get = "SELECT correct_answer, difficulty, type FROM questions WHERE id = ?";
//     sqlite3_stmt *stmt;
//     if (sqlite3_prepare_v2(db, sql_get, -1, &stmt, NULL) != SQLITE_OK) {
//         fprintf(stderr, "Erreur prepare SELECT: %s\n", sqlite3_errmsg(db));
//         return -1;
//     }
//     sqlite3_bind_int(stmt, 1, question_id);

//     char correct_answer[1024] = {0};
//     char difficulty[32] = {0};
//     char type[32] = {0};

//     if (sqlite3_step(stmt) == SQLITE_ROW) {
//         const unsigned char *c = sqlite3_column_text(stmt, 0);
//         const unsigned char *d = sqlite3_column_text(stmt, 1);
//         const unsigned char *t = sqlite3_column_text(stmt, 2);
//         if (c) strncpy(correct_answer, (const char *)c, sizeof(correct_answer)-1);
//         if (d) strncpy(difficulty, (const char *)d, sizeof(difficulty)-1);
//         if (t) strncpy(type, (const char *)t, sizeof(type)-1);
//     } else {
//         sqlite3_finalize(stmt);
//         fprintf(stderr, "Question non trouvée (id=%d)\n", question_id);
//         return -1;
//     }
//     sqlite3_finalize(stmt);

//     // Affichage debug
//     printf("player_answer: %s\n", player_answer);
//     printf("correct_answer (raw): %s\n", correct_answer);
//     printf("type (type): %s\n", type);

//     // Vérifier si c'est un QCM ou boolean (stocké en ["..."])
//     if (correct_answer[0] == '[') {
//         // Nettoyer la chaîne JSON
//         char clean[1024] = {0};
//         int j = 0;
//         for (int i = 0; correct_answer[i] != '\0' && j < sizeof(clean)-1; i++) {
//             if (correct_answer[i] != '[' && correct_answer[i] != ']' && correct_answer[i] != '"') {
//                 clean[j++] = correct_answer[i];
//             }
//         }
//         clean[j] = '\0';

//         // Tableau des bonnes réponses séparées par des virgules
//         char *correct_copy = strdup(clean);
//         char *token;
//         bool all_found = true;

//         token = strtok(correct_copy, ",");
//         while (token) {
//             // Supprimer espaces en début et fin
//             while (*token == ' ') token++;
//             char *end = token + strlen(token) - 1;
//             while (end > token && *end == ' ') *end-- = '\0';

//             if (!strstr(player_answer, token)) {
//                 all_found = false;
//                 break;
//             }
//             token = strtok(NULL, ",");
//         }
//         free(correct_copy);
//         correct = all_found ? 1 : 0;
//     } else {
//         // Text ou boolean simple
//         char clean[512] = {0};
//         int j = 0;
//         for (int i = 0; correct_answer[i] != '\0' && j < sizeof(clean)-1; i++) {
//             if (correct_answer[i] != '[' && correct_answer[i] != ']' && correct_answer[i] != '"') {
//                 clean[j++] = correct_answer[i];
//             }
//         }
//         clean[j] = '\0';
//         correct = (strcmp(player_answer, clean) == 0) ? 1 : 0;
//     }

//     // Calcul points selon difficulté
//     if (correct) {
//         if (strcmp(difficulty, "facile") == 0) points = 10;
//         else if (strcmp(difficulty, "moyen") == 0) points = 20;
//         else if (strcmp(difficulty, "difficile") == 0) points = 30;
//     }

//     // 3. Insérer la réponse
//     const char *sql_insert =
//         "INSERT INTO answers (players_session_id, question_id, answer, response_time, correct, points) "
//         "VALUES (?, ?, ?, ?, ?, ?);";

//     if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL) != SQLITE_OK) {
//         fprintf(stderr, "Erreur prepare INSERT: %s\n", sqlite3_errmsg(db));
//         return -1;
//     }

//     sqlite3_bind_int(stmt, 1, players_session_id);
//     sqlite3_bind_int(stmt, 2, question_id);
//     sqlite3_bind_text(stmt, 3, player_answer, -1, SQLITE_TRANSIENT);
//     sqlite3_bind_double(stmt, 4, response_time);
//     sqlite3_bind_int(stmt, 5, correct);
//     sqlite3_bind_int(stmt, 6, points);

//     int rc = sqlite3_step(stmt);
//     if (rc != SQLITE_DONE) {
//         fprintf(stderr, "Erreur INSERT: %s\n", sqlite3_errmsg(db));
//         sqlite3_finalize(stmt);
//         return -1;
//     }

//     sqlite3_finalize(stmt);
//     return 0;
// }

// stocke la réponse d'un joueur pour une question
// int store_player_answer(sqlite3 *db, int players_session_id, int question_id, const char *player_answer, double response_time) {
//     if (!db || !player_answer) return -1;

//     int correct = 0;
//     int points = 0;

//     // 1. Récupérer la réponse correcte depuis la table questions
//     const char *sql_get = "SELECT correct_answer, difficulty FROM questions WHERE id = ?";
//     sqlite3_stmt *stmt;
//     if (sqlite3_prepare_v2(db, sql_get, -1, &stmt, NULL) != SQLITE_OK) {
//         fprintf(stderr, "Erreur prepare store_player_answer SELECT: %s\n", sqlite3_errmsg(db));
//         return -1;
//     }
//     sqlite3_bind_int(stmt, 1, question_id);

//     char correct_answer[512] = {0};
//     char difficulty[32] = {0};

//     if (sqlite3_step(stmt) == SQLITE_ROW) {
//         const unsigned char *c = sqlite3_column_text(stmt, 0);
//         const unsigned char *d = sqlite3_column_text(stmt, 1);
//         if (c) strncpy(correct_answer, (const char *)c, sizeof(correct_answer)-1);
//         if (d) strncpy(difficulty, (const char *)d, sizeof(difficulty)-1);
//     } else {
//         sqlite3_finalize(stmt);
//         fprintf(stderr, "Question non trouvée (id=%d)\n", question_id);
//         return -1;
//     }
//     sqlite3_finalize(stmt);
//     int cmp = strcmp(player_answer, correct_answer);

//     // Affichage des deux valeurs et du résultat de strcmp
//     printf("player_answer: %s\n", player_answer);
//     printf("correct_answer: %s\n", correct_answer);
//     printf("strcmp(player_answer, correct_answer) = %d\n", cmp);
//     // 2. Comparer la réponse du joueur avec la réponse correcte
//     if (cmp == 0) {
//         correct = 1;

//         // Calculer les points selon la difficulté
//         if (strcmp(difficulty, "facile") == 0) points = 10;
//         else if (strcmp(difficulty, "moyen") == 0) points = 20;
//         else if (strcmp(difficulty, "difficile") == 0) points = 30;
//     } else {
//         correct = 0;
//         points = 0;
//     }

//     // 3. Insérer la réponse dans la table answers
//     const char *sql_insert =
//         "INSERT INTO answers (players_session_id, question_id, answer, response_time, correct, points) "
//         "VALUES (?, ?, ?, ?, ?, ?);";
    
//     if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, NULL) != SQLITE_OK) {
//         fprintf(stderr, "Erreur prepare store_player_answer INSERT: %s\n", sqlite3_errmsg(db));
//         return -1;
//     }

//     sqlite3_bind_int(stmt, 1, players_session_id);
//     sqlite3_bind_int(stmt, 2, question_id);
//     sqlite3_bind_text(stmt, 3, player_answer, -1, SQLITE_TRANSIENT);
//     sqlite3_bind_double(stmt, 4, response_time);
//     sqlite3_bind_int(stmt, 5, correct);
//     sqlite3_bind_int(stmt, 6, points);

//     int rc = sqlite3_step(stmt);
//     if (rc != SQLITE_DONE) {
//         fprintf(stderr, "Erreur store_player_answer INSERT: %s\n", sqlite3_errmsg(db));
//         sqlite3_finalize(stmt);
//         return -1;
//     }

//     sqlite3_finalize(stmt);
//     return 0;
// }


int load_session_results(sqlite3 *db, int session_id, PlayerResult *results) {
    sqlite3_stmt *stmt;

    const char *sql =
        "SELECT "
        "  p.pseudo, "
        "  COALESCE(a.answer, '-1') AS answer, "
        "  COALESCE(a.correct, 0) AS correct, "
        "  COALESCE(a.points, 0) AS points, "
        "  ps.current_score AS total_score, "
        "  COALESCE(a.response_time, 0) AS response_time, "
        "  ps.lives, "
        "  CASE WHEN a.id IS NOT NULL THEN 1 ELSE 0 END AS has_answer "
        "FROM players_session ps "
        "JOIN players p ON p.id = ps.player_id "
        "LEFT JOIN answers a ON a.players_session_id = ps.id "
        "  AND a.question_id = ( "
        "      SELECT question_id "
        "      FROM session_questions "
        "      WHERE session_id = ? "
        "        AND num_question = ( "
        "            SELECT current_question "
        "            FROM sessions WHERE id = ? "
        "        ) "
        "  ) "
        "WHERE ps.session_id = ? "
        "ORDER BY ps.current_score DESC";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "load_session_results prepare error: %s\n",
                sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, session_id);
    sqlite3_bind_int(stmt, 2, session_id);
    sqlite3_bind_int(stmt, 3, session_id);

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {

        strncpy(results[i].pseudo,
                (const char *)sqlite3_column_text(stmt, 0),
                sizeof(results[i].pseudo) - 1);

        strncpy(results[i].answer,
                (const char *)sqlite3_column_text(stmt, 1),
                sizeof(results[i].answer) - 1);

        results[i].correct = sqlite3_column_int(stmt, 2);
        results[i].points = sqlite3_column_int(stmt, 3);
        results[i].totalScore = sqlite3_column_int(stmt, 4);
        results[i].responseTime = sqlite3_column_double(stmt, 5);
        results[i].lives = sqlite3_column_int(stmt, 6);
        results[i].has_answer = sqlite3_column_int(stmt, 7) != 0; // has_answer

        i++;
    }

    sqlite3_finalize(stmt);
    return i; // nombre de joueurs
}

// int load_session_results(sqlite3 *db, int session_id, PlayerResult *results) {
//     sqlite3_stmt *stmt;
//     const char *sql = "SELECT pseudo, answer, correct, points, total_score, response_time, lives "
//                       "FROM player_results WHERE session_id = ?";

//     if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
//         fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
//         return 0;
//     }

//     sqlite3_bind_int(stmt, 1, session_id);

//     int i = 0;
//     while (sqlite3_step(stmt) == SQLITE_ROW) {
//         strncpy(results[i].pseudo, (const char *)sqlite3_column_text(stmt, 0), sizeof(results[i].pseudo)-1);
//         strncpy(results[i].answer, (const char *)sqlite3_column_text(stmt, 1), sizeof(results[i].answer)-1);
//         results[i].correct = sqlite3_column_int(stmt, 2);
//         results[i].points = sqlite3_column_int(stmt, 3);
//         results[i].totalScore = sqlite3_column_int(stmt, 4);
//         results[i].responseTime = sqlite3_column_double(stmt, 5);
//         results[i].lives = sqlite3_column_int(stmt, 6);
//         i++;
//     }

//     sqlite3_finalize(stmt);
//     return i; // nombre de joueurs chargés
// }


bool session_is_battle(sqlite3 *db, int session_id) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT mode FROM sessions WHERE id = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, session_id);

    bool result = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *mode = sqlite3_column_text(stmt, 0);
        if (mode && strcmp((const char *)mode, "battle") == 0) {
            result = true;
        }
    }

    sqlite3_finalize(stmt);
    return result;
}


const char *get_correct_answer(sqlite3 *db, int session_id) {
    static char answer[256];
    sqlite3_stmt *stmt;
    const char *sql = "SELECT correct_answer FROM questions "
                      "WHERE id = (SELECT current_question_id FROM sessions WHERE id = ?)";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, session_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        strncpy(answer, (const char *)sqlite3_column_text(stmt, 0), sizeof(answer)-1);
    } else {
        answer[0] = '\0';
    }

    sqlite3_finalize(stmt);
    return answer;
}

const char *get_explanation(sqlite3 *db, int session_id) {
    static char explanation[512];
    sqlite3_stmt *stmt;
    const char *sql = "SELECT explanation FROM questions "
                      "WHERE id = (SELECT current_question_id FROM sessions WHERE id = ?)";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, session_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        strncpy(explanation, (const char *)sqlite3_column_text(stmt, 0), sizeof(explanation)-1);
    } else {
        explanation[0] = '\0';
    }

    sqlite3_finalize(stmt);
    return explanation;
}


const char *compute_last_player(PlayerResult *results, int count) {
    if (count <= 0) return NULL;
    
    // Trouver le joueur avec le temps de réponse le plus élevé (le dernier à avoir répondu)
    // Ignorer les joueurs qui n'ont pas répondu (timeout) et ceux qui ont utilisé le joker Passer
    int last_idx = -1;
    double max_time = -1.0;
    
    for (int i = 0; i < count; i++) {
        // Ignorer les joueurs qui n'ont pas répondu (timeout) ou qui ont utilisé le joker Passer
        // has_answer=false ET answer="-1" = timeout (ignorer)
        // has_answer=true ET answer="-1" = joker Passer (ignorer aussi pour le calcul du lastPlayer)
        bool is_timeout = (strcmp(results[i].answer, "-1") == 0 && !results[i].has_answer);
        bool is_skip_joker = (strcmp(results[i].answer, "-1") == 0 && results[i].has_answer);
        
        if (!is_timeout && !is_skip_joker && results[i].responseTime > max_time) {
            max_time = results[i].responseTime;
            last_idx = i;
        }
    }
    
    if (last_idx < 0) return NULL; // Aucun joueur n'a vraiment répondu
    
    return results[last_idx].pseudo;
}

// int get_current_question_id(int player_id, sqlite3 *db) {
//     int question_id = -1;
//     sqlite3_stmt *stmt;

//     const char *sql = 
//         "SELECT s.current_question "
//         "FROM players_session ps "
//         "JOIN sessions s ON ps.session_id = s.id "
//         "WHERE ps.player_id = ? "
//         "AND s.status = 'playing' "
//         "ORDER BY s.id DESC "
//         "LIMIT 1;";

//     if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
//         fprintf(stderr, "[ERROR] Failed to prepare statement: %s\n", sqlite3_errmsg(db));
//         return -1;
//     }

//     sqlite3_bind_int(stmt, 1, player_id);

//     if (sqlite3_step(stmt) == SQLITE_ROW) {
//         question_id = sqlite3_column_int(stmt, 0);
//         printf("[DEBUG] Current question for player_id=%d in latest session is %d\n", player_id, question_id);
//     } else {
//         printf("[WARN] No active playing session found for player_id=%d\n", player_id);
//     }

//     sqlite3_finalize(stmt);
//     return (question_id-1);
// }
int get_current_question_id(int player_id, sqlite3 *db)
{
    int question_id = -1;
    int session_id = -1;
    int num_question = -1;

    sqlite3_stmt *stmt;

    const char *sql =
        "SELECT s.id AS session_id, "
        "       s.current_question AS num_question, "
        "       sq.question_id "
        "FROM players_session ps "
        "JOIN sessions s ON ps.session_id = s.id "
        "JOIN session_questions sq "
        "     ON sq.session_id = s.id "
        "    AND sq.num_question = s.current_question "
        "WHERE ps.player_id = ? "
        "  AND s.status = 'playing' "
        "ORDER BY s.id DESC "
        "LIMIT 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr,
            "[ERROR] SQL prepare failed: %s\n",
            sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_int(stmt, 1, player_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        session_id   = sqlite3_column_int(stmt, 0);
        num_question = sqlite3_column_int(stmt, 1);
        question_id  = sqlite3_column_int(stmt, 2);

        printf(
            "[DEBUG] get_current_question_id():\n"
            "        session_id   = %d\n"
            "        num_question = %d\n"
            "        question_id  = %d\n",
            session_id,
            num_question,
            question_id
        );
    } else {
        printf(
            "[WARN] No active playing session found for player_id=%d\n",
            player_id
        );
    }

    sqlite3_finalize(stmt);
    return question_id;
}

// int get_current_question_id(int player_id, sqlite3 *db)
// {
//     int question_id = -1;
//     sqlite3_stmt *stmt;

//     const char *sql =
//         "SELECT sq.question_id "
//         "FROM players_session ps "
//         "JOIN sessions s ON ps.session_id = s.id "
//         "JOIN session_questions sq "
//         "  ON sq.session_id = s.id "
//         " AND sq.num_question = s.current_question "
//         "WHERE ps.player_id = ? "
//         "  AND s.status = 'playing' "
//         "ORDER BY s.id DESC "
//         "LIMIT 1;";

//     if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
//         fprintf(stderr, "[ERROR] get_current_question_id prepare: %s\n",
//                 sqlite3_errmsg(db));
//         return -1;
//     }

//     sqlite3_bind_int(stmt, 1, player_id);

//     if (sqlite3_step(stmt) == SQLITE_ROW) {
//         question_id = sqlite3_column_int(stmt, 0);
//         printf("[DEBUG] Current question_id=%d for player_id=%d\n",
//                question_id, player_id);
//     } else {
//         printf("[WARN] No current question found for player_id=%d\n", player_id);
//     }

//     sqlite3_finalize(stmt);
//     return question_id;
// }
