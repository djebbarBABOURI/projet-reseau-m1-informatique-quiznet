#include "handlers.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "sqlite/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include "db_utils.h"
#include "utils.h"
#include "clients.h"
//
bool request_is_complete(const char *buffer)
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

// 
bool extract_json_string(const char *json, const char *key, char *out, size_t outSize)
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

//
void handle_client_request(const char *request, int client_fd, sqlite3 *db)
{
    char method[8] = {0};
    char path[128] = {0};

    printf("DEBUG: Requête reçue: %.*s\n", (int)(strlen(request) < 200 ? strlen(request) : 200), request);

    if (sscanf(request, "%7s %127s", method, path) != 2) {
        send_error_response(client_fd, "unknown", "400", "Bad request");
        return;
    }

    // Nettoyer le path : supprimer les caractères de fin de ligne et les slashes au début
    size_t path_len = strlen(path);
    while (path_len > 0 && (path[path_len - 1] == '\n' || path[path_len - 1] == '\r' || path[path_len - 1] == ' ')) {
        path[path_len - 1] = '\0';
        path_len--;
    }
    // Supprimer le slash au début si présent
    if (path[0] == '/') {
        memmove(path, path + 1, strlen(path));
    }
    
    printf("DEBUG: Method='%s', Path='%s'\n", method, path);

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
            int player_id = get_player_id_by_pseudo(db, pseudo);
            if (player_id > 0) {
                add_authenticated_client(client_fd, player_id);
                print_authenticated_clients();
            }
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
        int player_id = get_player_id_by_pseudo(db, pseudo);
        if (player_id > 0) {
            add_authenticated_client(client_fd, player_id);
            print_authenticated_clients();
        }
        if (rc == 0) {
            send_login_success(client_fd);
        } else if (rc == 1) {
            send_error_response(client_fd, "player/login", "401", "invalid credentials");
        } else {
            send_error_response(client_fd, "player/login", "520", "Unknown Error");
        }
        return;
    }

    if (strcasecmp(method, "GET") == 0 && strcmp(path, "themes/list") == 0) {
        handle_themes_list(client_fd, db);
        return;
    }

    if (strcasecmp(method, "GET") == 0 && strcmp(path, "sessions/list") == 0) {
        printf("DEBUG: Requête GET sessions/list reçue, path='%s'\n", path);
        handle_sessions_list(client_fd, db);
        return;
    }

    if (strcasecmp(method, "POST") == 0 && strcmp(path, "session/create") == 0) {
        if (!json) {
            send_error_response(client_fd, "session/create", "400", "Missing JSON body");
            return;
        }
        handle_session_create(json, client_fd, db);
        return;
    }

    if (strcasecmp(method, "POST") == 0 && strcmp(path, "session/join") == 0) {
        if (!json) {
            send_error_response(client_fd, "session/join", "400", "Missing JSON body");
            return;
        }

        // Extraire sessionId
        int session_id = 0;
        if (!extract_json_int(json, "sessionId", &session_id)) {
            send_error_response(client_fd, "session/join", "400", "Missing or invalid sessionId");
            return;
        }

        // Récupérer l'ID du joueur à partir du client_fd
        int player_id = get_player_id_by_fd(client_fd); // clients.c
        if (player_id <= 0) {
            send_error_response(client_fd, "session/join", "401", "Player not authenticated");
            return;
        }

        // Vérifier que la session existe
        sqlite3_stmt *stmt;
        const char *sql = "SELECT mode, max_players, status, created_by FROM sessions WHERE id = ?";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            send_error_response(client_fd, "session/join", "500", "Database error");
            return;
        }
        sqlite3_bind_int(stmt, 1, session_id);

        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            send_error_response(client_fd, "session/join", "404", "Session not found");
            return;
        }

        const char *mode = (const char *)sqlite3_column_text(stmt, 0);
        int max_players = sqlite3_column_int(stmt, 1);
        const char *status = (const char *)sqlite3_column_text(stmt, 2);
        int creator_id = sqlite3_column_int(stmt, 3);

        sqlite3_finalize(stmt);
        
        // Vérifier si le joueur est déjà dans la session
        const char *sql_check = "SELECT id FROM players_session WHERE player_id = ? AND session_id = ?";
        sqlite3_stmt *stmt_check;
        bool already_in_session = false;
        
        if (sqlite3_prepare_v2(db, sql_check, -1, &stmt_check, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt_check, 1, player_id);
            sqlite3_bind_int(stmt_check, 2, session_id);
            if (sqlite3_step(stmt_check) == SQLITE_ROW) {
                already_in_session = true;
            }
        }
        sqlite3_finalize(stmt_check);
        
        // Si le joueur est déjà dans la session, renvoyer une réponse de succès
        if (already_in_session) {
            // Construire la réponse comme si le joueur venait de rejoindre
            char mode_str[16];
            if (!get_session_mode(db, session_id, mode_str, sizeof(mode_str))) {
                send_error_response(client_fd, "session/join", "404", "Session not found");
                return;
            }
            
            bool is_creator = (creator_id == player_id);
            int lives = (strcmp(mode_str, "battle") == 0) ? 4 : 0;
            
            char response[1024];
            snprintf(response, sizeof(response),
                "{\"action\":\"session/join\",\"statut\":\"201\",\"message\":\"session joined\","
                "\"sessionId\":%d,\"mode\":\"%s\",\"isCreator\":%s,\"players\":[],\"lives\":%d,\"jokers\":{\"fifty\":1,\"skip\":1}}",
                session_id, mode_str, is_creator ? "true" : "false", lives);
            
            send(client_fd, response, strlen(response), 0);
            return;
        }
        
        // Si le créateur essaie de rejoindre (alors qu'il n'est pas dans la session), on l'ajoute quand même
        // Cela peut arriver si la session a été créée mais le joueur n'a pas été correctement ajouté
        
        // Vérifier si session complète
        int current_players = count_players_in_session(db, session_id);
        if (current_players >= max_players) {
            send_error_response(client_fd, "session/join", "403", "Session is full");
            return;
        }

        // Appeler join_session
        join_session(client_fd, db, player_id, session_id);

        return;
    }

    if (strcasecmp(method, "POST") == 0 && strcmp(path, "session/start") == 0) {
        printf("DEBUG: Requête POST session/start reçue de fd=%d\n", client_fd);
        handle_session_start(client_fd, db);
        return;
    }

    //--------------------------------------------------------------
    // POST question/answer
    //--------------------------------------------------------------
    if (strcasecmp(method, "POST") == 0 && strcmp(path, "question/answer") == 0) {
        if (!json) {
            send_error_response(client_fd, "question/answer", "400", "Missing JSON body");
            return;
        }
        handle_question_answer(json, client_fd, db);
        return;
    }

    //--------------------------------------------------------------
    // POST question/results
    //--------------------------------------------------------------
    if (strcasecmp(method, "POST") == 0 && strcmp(path, "question/results") == 0) {
        send_error_response(client_fd, "question/results", "403", "Results are server-only");
        return;
    }

    send_error_response(client_fd, "unknown", "404", "endpoint not implemented");
}

void handle_question_answer(const char *json, int client_fd, sqlite3 *db)
{
    // Récupération de l'ID du joueur
    int player_id = get_player_id_by_fd(client_fd);
    if (player_id <= 0) {
        printf("[WARN] Unauthenticated player tried to answer (fd=%d)\n", client_fd);
        send_error_response(client_fd, "question/answer", "401", "Player not authenticated");
        return;
    }
    printf("[INFO] Handling answer for player_id=%d\n", player_id);

    // Réponse : peut être int, bool ou string
    char answer_str[256] = {0};
    double response_time = 0.0;

    // Extraction de la réponse brute
    if (!extract_json_raw(json, "answer", answer_str, sizeof(answer_str))) {
        printf("[WARN] Missing answer from player_id=%d\n", player_id);
        send_error_response(client_fd, "question/answer", "400", "Missing answer");
        return;
    }

    // Extraction du temps de réponse
    if (!extract_json_double(json, "responseTime", &response_time)) {
        printf("[WARN] Invalid or missing responseTime from player_id=%d\n", player_id);
        send_error_response(client_fd, "question/answer", "400", "Missing or invalid responseTime");
        return;
    }

    // Récupérer l'ID de la question actuelle et l'ID de players_session
    int question_id = get_current_question_id(player_id, db);
    if (question_id <= 0) {
        printf("[WARN] No active question found for player_id=%d\n", player_id);
        send_error_response(client_fd, "question/answer", "404", "No active question found");
        return;
    }

    // Récupérer players_session_id pour ce joueur dans la dernière session playing
    int players_session_id = -1;
    sqlite3_stmt *stmt;
    const char *sql = 
        "SELECT ps.id "
        "FROM players_session ps "
        "JOIN sessions s ON ps.session_id = s.id "
        "WHERE ps.player_id = ? AND s.status = 'playing' "
        "ORDER BY s.id DESC LIMIT 1;";
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, player_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            players_session_id = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);

    if (players_session_id <= 0) {
        printf("[WARN] No active players_session found for player_id=%d\n", player_id);
        send_error_response(client_fd, "question/answer", "404", "No active session for player");
        return;
    }

    printf("[DEBUG] Player %d (players_session_id=%d) answered: '%s' in %.2f sec (question_id=%d)\n",
           player_id, players_session_id, answer_str, response_time, question_id);

    // Stocker la réponse dans la DB
    int store_result = store_player_answer(db, players_session_id, question_id, answer_str, response_time);
    if (store_result) {
        printf("[INFO] Answer stored successfully for players_session_id=%d\n", players_session_id);
    } else {
        printf("[ERROR] Failed to store answer for players_session_id=%d\n", players_session_id);
    }

    // Réponse standard au client
    const char *resp =
        "{\"action\":\"question/answer\",\"statut\":\"200\",\"message\":\"answer received\"}";
    ssize_t sent = send(client_fd, resp, strlen(resp), 0);
    if (sent < 0) {
        printf("[ERROR] Failed to send response to player_id=%d\n", player_id);
    } else {
        printf("[INFO] Response sent to player_id=%d (%zd bytes)\n", player_id, sent);
    }
}
// void handle_question_answer(const char *json, int client_fd, sqlite3 *db)
// {
//     // Récupération de l'ID du joueur
//     int player_id = get_player_id_by_fd(client_fd);
//     if (player_id <= 0) {
//         printf("[WARN] Unauthenticated player tried to answer (fd=%d)\n", client_fd);
//         send_error_response(client_fd, "question/answer", "401", "Player not authenticated");
//         return;
//     }
//     printf("[INFO] Handling answer for player_id=%d\n", player_id);

//     // Réponse : peut être int, bool ou string
//     char answer_str[256] = {0};
//     double response_time = 0.0;

//     // Extraction de la réponse brute
//     if (!extract_json_raw(json, "answer", answer_str, sizeof(answer_str))) {
//         printf("[WARN] Missing answer from player_id=%d\n", player_id);
//         send_error_response(client_fd, "question/answer", "400", "Missing answer");
//         return;
//     }

//     // Extraction du temps de réponse
//     if (!extract_json_double(json, "responseTime", &response_time)) {
//         printf("[WARN] Invalid or missing responseTime from player_id=%d\n", player_id);
//         send_error_response(client_fd, "question/answer", "400", "Missing or invalid responseTime");
//         return;
//     }

//     // Récupérer l'ID de la question actuelle
//     int question_id = get_current_question_id(player_id, db);
//     if (question_id <= 0) {
//         printf("[WARN] No active question found for player_id=%d\n", player_id);
//         send_error_response(client_fd, "question/answer", "404", "No active question found");
//         return;
//     }

//     printf("[DEBUG] Player %d answered: '%s' in %.2f sec (question_id=%d)\n",
//            player_id, answer_str, response_time, question_id);

//     // Stocker la réponse dans la DB
//     int store_result = store_player_answer(db, player_id, question_id, answer_str, response_time);
//     if (store_result) {
//         printf("[INFO] Answer from player_id=%d stored successfully\n", player_id);
//     } else {
//         printf("[ERROR] Failed to store answer from player_id=%d\n", player_id);
//     }

//     // Réponse standard au client
//     const char *resp =
//         "{\"action\":\"question/answer\",\"statut\":\"200\",\"message\":\"answer received\"}";
//     ssize_t sent = send(client_fd, resp, strlen(resp), 0);
//     if (sent < 0) {
//         printf("[ERROR] Failed to send response to player_id=%d\n", player_id);
//     } else {
//         printf("[INFO] Response sent to player_id=%d (%zd bytes)\n", player_id, sent);
//     }
// }

// void handle_question_answer(const char *json, int client_fd, sqlite3 *db)
// {
//     int player_id = get_player_id_by_fd(client_fd);
//     if (player_id <= 0) {
//         send_error_response(client_fd, "question/answer", "401", "Player not authenticated");
//         return;
//     }

//     // Réponse : peut être int, bool ou string
//     char answer_str[256] = {0};
//     double response_time = 0.0;

//     // on récupère "answer":... SANS savoir le type
//     if (!extract_json_raw(json, "answer", answer_str, sizeof(answer_str))) {
//         send_error_response(client_fd, "question/answer", "400", "Missing answer");
//         return;
//     }

//     if (!extract_json_double(json, "responseTime", &response_time)) {
//         send_error_response(client_fd, "question/answer", "400", "Missing or invalid responseTime");
//         return;
//     }

//     // Récupérer l'ID de la question en cours pour ce joueur / session
//     int question_id = get_current_question_id(player_id, db);
//     if (question_id <= 0) {
//         send_error_response(client_fd, "question/answer", "404", "No active question found");
//         return;
//     }

//     printf("[DEBUG] Player %d answered: %s in %.2f sec for question %d\n", player_id, answer_str, response_time, question_id);

//     // Stocker la réponse dans la DB
//     store_player_answer(db, player_id, question_id, answer_str, response_time);

//     // Réponse standard
//     const char *resp =
//         "{\"action\":\"question/answer\",\"statut\":\"200\",\"message\":\"answer received\"}";
//     send(client_fd, resp, strlen(resp), 0);
// }

void send_question_results(int session_id, sqlite3 *db)
{
    // Récupérer le max_players depuis la table sessions
    int max_players = 0;
    sqlite3_stmt *stmt;
    const char *sql = "SELECT max_players FROM sessions WHERE id = ?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, session_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            max_players = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);

    if (max_players <= 0) {
        printf("DEBUG send_question_results: session %d max_players invalide, utilisation d'une valeur par défaut 10\n", session_id);
        max_players = 10; // valeur par défaut si la session n'existe pas ou max_players invalide
    }

    // Charger toutes les réponses
    PlayerResult results[max_players];
    int count = load_session_results(db, session_id, results);

    bool is_battle = session_is_battle(db, session_id);

    char json[4096];
    size_t len = 0;

    len += sprintf(json + len,
        "{\"action\":\"question/results\",\"statut\":\"200\","
        "\"correctAnswer\":%d,"
        "\"explanation\":\"%s\",",
        get_correct_answer(db, session_id),
        get_explanation(db, session_id)
    );

    if (is_battle) {
        const char *last = compute_last_player(results, count);
        len += sprintf(json + len, "\"lastPlayer\":\"%s\",", last);
    }

    len += sprintf(json + len, "\"results\":[");

    for (int i = 0; i < count; i++) {
        len += sprintf(json + len,
            "%s{\"pseudo\":\"%s\",\"answer\":%d,\"correct\":%s,"
            "\"points\":%d,\"totalScore\":%d",
            i == 0 ? "" : ",",
            results[i].pseudo,
            results[i].answer,
            results[i].correct ? "true" : "false",
            results[i].points,
            results[i].totalScore
        );
        if (is_battle) {
            len += sprintf(json + len, ",\"responseTime\":%.2f,\"lives\":%d",
                results[i].responseTime,
                results[i].lives
            );
        }
        len += sprintf(json + len, "}");
    }

    len += sprintf(json + len, "]}");

    // Broadcast aux joueurs de la session
    broadcast_to_session(db, session_id, json);
}

void send_player_eliminated(sqlite3 *db, int session_id, const char *pseudo)
{
    char json[256];
    snprintf(json, sizeof(json),
        "{\"action\":\"session/player/eliminated\",\"pseudo\":\"%s\"}", pseudo);

    // Appel avec le db en premier paramètre
    broadcast_to_session(db, session_id, json);
}

//
void handle_themes_list(int fd, sqlite3 *db)
{
    const char *sql = "SELECT id, name FROM themes ORDER BY id;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        send_error_response(fd, "themes/list", "500", "database error");
        return;
    }

    size_t capacity = 2048;
    size_t length = 0;
    char *json = malloc(capacity);
    if (!json) {
        sqlite3_finalize(stmt);
        send_error_response(fd, "themes/list", "500", "memory error");
        return;
    }
    json[0] = '\0';

    bool ok = append_str(&json, &length, &capacity, "{\"action\":\"themes/list\",\"statut\":\"200\",\"themes\":[");
    bool first = true;

    while (ok) {
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            if (!first) {
                ok = append_char(&json, &length, &capacity, ',');
                if (!ok) break;
            }
            first = false;
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char *name = sqlite3_column_text(stmt, 1);

            char id_buf[32];
            snprintf(id_buf, sizeof(id_buf), "%d", id);

            ok = append_str(&json, &length, &capacity, "{\"id\":");
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, id_buf);
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, ",\"name\":\"");
            if (!ok) break;
            ok = append_json_string(&json, &length, &capacity, name ? name : (const unsigned char *)"");
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, "\"}");
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            ok = false;
            break;
        }
    }

    sqlite3_finalize(stmt);

    if (!ok) {
        free(json);
        send_error_response(fd, "themes/list", "500", "internal error");
        return;
    }

    if (!append_str(&json, &length, &capacity, "]}\n")) {
        free(json);
        send_error_response(fd, "themes/list", "500", "internal error");
        return;
    }

    send(fd, json, length, 0);
    free(json);
}


//
void handle_sessions_list(int fd, sqlite3 *db)
{
    printf("DEBUG: handle_sessions_list appelé\n");
    // Requête SQL pour récupérer toutes les sessions avec le nombre de joueurs
    const char *sql = 
        "SELECT s.id, s.name, s.difficulty, s.nb_questions, s.time_limit, "
        "s.mode, s.max_players, s.status, "
        "COALESCE(COUNT(ps.id), 0) as nb_players "
        "FROM sessions s "
        "LEFT JOIN players_session ps ON s.id = ps.session_id "
        "WHERE s.status = 'waiting' OR s.status = 'playing' "
        "GROUP BY s.id, s.name, s.difficulty, s.nb_questions, s.time_limit, s.mode, s.max_players, s.status "
        "ORDER BY s.id;";
    
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "DEBUG: Erreur prepare SQL: %s\n", sqlite3_errmsg(db));
        send_error_response(fd, "sessions/list", "500", "database error");
        return;
    }
    printf("DEBUG: Requête SQL préparée avec succès\n");

    size_t capacity = 4096;
    size_t length = 0;
    char *json = malloc(capacity);
    if (!json) {
        sqlite3_finalize(stmt);
        send_error_response(fd, "sessions/list", "500", "memory error");
        return;
    }
    json[0] = '\0';

    bool ok = append_str(&json, &length, &capacity, "{\"action\":\"sessions/list\",\"statut\":\"200\",\"sessions\":[");
    bool first = true;
    int row_count = 0;

    while (ok) {
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            row_count++;
            if (!first) {
                ok = append_char(&json, &length, &capacity, ',');
                if (!ok) break;
            }
            first = false;
            
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char *name = sqlite3_column_text(stmt, 1);
            const unsigned char *difficulty = sqlite3_column_text(stmt, 2);
            int nb_questions = sqlite3_column_int(stmt, 3);
            int time_limit = sqlite3_column_int(stmt, 4);
            const unsigned char *mode = sqlite3_column_text(stmt, 5);
            int max_players = sqlite3_column_int(stmt, 6);
            // const unsigned char *status = sqlite3_column_text(stmt, 7);
            const unsigned char *status_raw = sqlite3_column_text(stmt, 7);
            const unsigned char *status = status_raw ? status_raw : (const unsigned char*)"waiting";
            int nb_players = sqlite3_column_int(stmt, 8);

            char id_buf[32], nb_questions_buf[32], time_limit_buf[32], max_players_buf[32], nb_players_buf[32];
            snprintf(id_buf, sizeof(id_buf), "%d", id);
            snprintf(nb_questions_buf, sizeof(nb_questions_buf), "%d", nb_questions);
            snprintf(time_limit_buf, sizeof(time_limit_buf), "%d", time_limit);
            snprintf(max_players_buf, sizeof(max_players_buf), "%d", max_players);
            snprintf(nb_players_buf, sizeof(nb_players_buf), "%d", nb_players);

            ok = append_str(&json, &length, &capacity, "{\"id\":");
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, id_buf);
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, ",\"name\":\"");
            if (!ok) break;
            ok = append_json_string(&json, &length, &capacity, name ? name : (const unsigned char *)"");
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, "\",\"difficulty\":\"");
            if (!ok) break;
            ok = append_json_string(&json, &length, &capacity, difficulty ? difficulty : (const unsigned char *)"");
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, "\",\"nbQuestions\":");
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, nb_questions_buf);
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, ",\"timeLimit\":");
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, time_limit_buf);
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, ",\"mode\":\"");
            if (!ok) break;
            ok = append_json_string(&json, &length, &capacity, mode ? mode : (const unsigned char *)"");
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, "\",\"maxPlayers\":");
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, max_players_buf);
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, ",\"nbPlayers\":");
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, nb_players_buf);
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, ",\"status\":\"");
            if (!ok) break;
            ok = append_json_string(&json, &length, &capacity, status ? status : (const unsigned char *)"");
            if (!ok) break;
            ok = append_str(&json, &length, &capacity, "\"}");
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            ok = false;
            break;
        }
    }

    sqlite3_finalize(stmt);

    printf("DEBUG: Nombre de sessions trouvées: %d\n", row_count);

    if (!ok) {
        free(json);
        send_error_response(fd, "sessions/list", "500", "internal error");
        return;
    }

    if (!append_str(&json, &length, &capacity, "]}\n")) {
        free(json);
        send_error_response(fd, "sessions/list", "500", "internal error");
        return;
    }

    printf("DEBUG: Réponse JSON générée, longueur: %zu\n", length);
    printf("DEBUG: Réponse JSON: %.*s\n", (int)(length < 500 ? length : 500), json);
    send(fd, json, length, 0);
    free(json);
    printf("DEBUG: Réponse envoyée au client\n");
}

// Structure pour passer les arguments au thread de démarrage de session
typedef struct {
    sqlite3 *db;
    int session_id;
    int nb_questions;
} SessionStartArgs;

// Diffuse un message JSON à tous les joueurs d'une session
void broadcast_to_session(sqlite3 *db, int session_id, const char *message) {
    int fds[100];
    int count = get_session_fds(db, session_id, fds, 100);
    printf("DEBUG broadcast_to_session: session_id=%d, nb_joueurs=%d\n", session_id, count);
    
    for (int i = 0; i < count; i++) {
        printf("DEBUG broadcast_to_session: envoi à fd=%d\n", fds[i]);
        ssize_t sent = send(fds[i], message, strlen(message), 0);
        if (sent < 0) {
            printf("DEBUG broadcast_to_session: erreur envoi à fd=%d\n", fds[i]);
        } else {
            printf("DEBUG broadcast_to_session: %zd bytes envoyés à fd=%d\n", sent, fds[i]);
        }
    }
}

static void send_next_question(sqlite3 *db, int session_id) {
    // Récupérer le numéro de la question courante et nb_questions
    const char *sql_session = "SELECT current_question, nb_questions, time_limit FROM sessions WHERE id = ?";
    sqlite3_stmt *stmt_session;
    int current_question = 0; // départ à 0 pour la première question
    int nb_questions = 0;
    int time_limit = 30;

    if (sqlite3_prepare_v2(db, sql_session, -1, &stmt_session, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt_session, 1, session_id);
        if (sqlite3_step(stmt_session) == SQLITE_ROW) {
            current_question = sqlite3_column_int(stmt_session, 0);
            nb_questions = sqlite3_column_int(stmt_session, 1);
            time_limit = sqlite3_column_int(stmt_session, 2);
        }
    }
    sqlite3_finalize(stmt_session);

    if (current_question >= nb_questions) { // >= car current_question commence à 0
        printf("DEBUG send_next_question: toutes les questions ont déjà été envoyées\n");
        return;
    }

    int question_num = current_question + 1; // décaler pour récupérer la question correcte

    // Récupérer la question courante
    const char *sql_question =
        "SELECT q.id, q.type, q.enonce, q.answers, q.difficulty "
        "FROM session_questions sq "
        "JOIN questions q ON sq.question_id = q.id "
        "WHERE sq.session_id = ? AND sq.num_question = ?";
    sqlite3_stmt *stmt_question;

    if (sqlite3_prepare_v2(db, sql_question, -1, &stmt_question, NULL) != SQLITE_OK) return;
    sqlite3_bind_int(stmt_question, 1, session_id);
    sqlite3_bind_int(stmt_question, 2, question_num);

    if (sqlite3_step(stmt_question) == SQLITE_ROW) {
        const unsigned char *type = sqlite3_column_text(stmt_question, 1);
        const unsigned char *enonce = sqlite3_column_text(stmt_question, 2);
        const unsigned char *answers_json = sqlite3_column_text(stmt_question, 3);
        const unsigned char *difficulty = sqlite3_column_text(stmt_question, 4);

        // Construire le JSON
        size_t capacity = 2048, length = 0;
        char *json = malloc(capacity);
        if (!json) { sqlite3_finalize(stmt_question); return; }
        json[0] = '\0';

        bool ok = append_str(&json, &length, &capacity, "POST question/new\n{");
        ok = ok && append_str(&json, &length, &capacity, "\"action\":\"question/new\",");
        char buf[64];
        snprintf(buf, sizeof(buf), "\"questionNum\":%d,\"totalQuestions\":%d", question_num, nb_questions);
        ok = ok && append_str(&json, &length, &capacity, buf);
        ok = ok && append_str(&json, &length, &capacity, ",\"type\":\"");
        ok = ok && append_json_string(&json, &length, &capacity, type);
        ok = ok && append_str(&json, &length, &capacity, "\",\"difficulty\":\"");
        ok = ok && append_json_string(&json, &length, &capacity, difficulty);
        ok = ok && append_str(&json, &length, &capacity, "\",\"question\":\"");
        ok = ok && append_json_string(&json, &length, &capacity, enonce);
        ok = ok && append_str(&json, &length, &capacity, "\"");
        ok = ok && append_str(&json, &length, &capacity, ",\"timeLimit\":");
        snprintf(buf, sizeof(buf), "%d", time_limit);
        ok = ok && append_str(&json, &length, &capacity, buf);

        if (type && strcmp((const char *)type, "qcm") == 0 && answers_json) {
            ok = ok && append_str(&json, &length, &capacity, ",\"answers\":");
            ok = ok && append_str(&json, &length, &capacity, (const char *)answers_json);
        }

        ok = ok && append_str(&json, &length, &capacity, "}\n");

        if (ok) broadcast_to_session(db, session_id, json);
        free(json);

        // Incrémenter current_question dans la DB
        const char *sql_update = "UPDATE sessions SET current_question = ? WHERE id = ?";
        sqlite3_stmt *stmt_update;
        if (sqlite3_prepare_v2(db, sql_update, -1, &stmt_update, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt_update, 1, question_num); // stocke la question envoyée
            sqlite3_bind_int(stmt_update, 2, session_id);
            sqlite3_step(stmt_update);
        }
        sqlite3_finalize(stmt_update);
    }

    sqlite3_finalize(stmt_question);
}

void send_session_finished_solo(int session_id, sqlite3 *db) {
    const char *sql =
        "SELECT p.pseudo, ps.current_score, COALESCE(SUM(a.correct),0) "
        "FROM players_session ps "
        "JOIN players p ON p.id = ps.player_id "
        "LEFT JOIN answers a ON a.players_session_id = ps.id "
        "WHERE ps.session_id = ? "
        "GROUP BY ps.id "
        "ORDER BY ps.current_score DESC, "
        "COALESCE(SUM(a.correct),0) DESC";

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, session_id);

    size_t capacity = 2048, length = 0;
    char *json = malloc(capacity);
    if (!json) {
        sqlite3_finalize(stmt);
        return;
    }
    json[0] = '\0';

    bool ok = true;

    ok = ok && append_str(&json, &length, &capacity,
    "POST session/finished\n{");
    ok = ok && append_str(&json, &length, &capacity,
    "\"action\":\"session/finished\","
    "\"mode\":\"solo\",\"ranking\":[");

    int rank = 1;
    bool first = true;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (!first)
            ok = ok && append_str(&json, &length, &capacity, ",");
        first = false;

        const char *pseudo = (const char *)sqlite3_column_text(stmt, 0);
        int score = sqlite3_column_int(stmt, 1);
        int correct = sqlite3_column_int(stmt, 2);

        char buf[128];
        snprintf(buf, sizeof(buf),
            "{\"rank\":%d,\"pseudo\":\"", rank++);
        ok = ok && append_str(&json, &length, &capacity, buf);
        ok = ok && append_json_string(&json, &length, &capacity, pseudo);
        snprintf(buf, sizeof(buf),
            "\",\"score\":%d,\"correctAnswers\":%d}", score, correct);
        ok = ok && append_str(&json, &length, &capacity, buf);
    }

    ok = ok && append_str(&json, &length, &capacity, "]}\n");

    sqlite3_finalize(stmt);

    if (ok) {
        broadcast_to_session(db, session_id, json);
    }

    free(json);
}
void send_session_finished_battle(int session_id, sqlite3 *db) {
    const char *sql =
        "SELECT p.pseudo, ps.lives, ps.current_score, "
        "COALESCE(SUM(a.correct),0), ps.eliminated_at "
        "FROM players_session ps "
        "JOIN players p ON p.id = ps.player_id "
        "LEFT JOIN answers a ON a.players_session_id = ps.id "
        "WHERE ps.session_id = ? "
        "GROUP BY ps.id "
        "ORDER BY ps.lives DESC, ps.eliminated_at DESC";

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, session_id);

    size_t capacity = 2048, length = 0;
    char *json = malloc(capacity);
    if (!json) {
        sqlite3_finalize(stmt);
        return;
    }
    json[0] = '\0';

    bool ok = true;
    char winner[64] = {0};

    ok = ok && append_str(&json, &length, &capacity,
    "POST session/finished\n{");
    ok = ok && append_str(&json, &length, &capacity,
    "\"action\":\"session/finished\","
    "\"mode\":\"battle\",\"ranking\":[");


    int rank = 1;
    bool first = true;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *pseudo = (const char *)sqlite3_column_text(stmt, 0);
        int lives = sqlite3_column_int(stmt, 1);
        int score = sqlite3_column_int(stmt, 2);
        int correct = sqlite3_column_int(stmt, 3);
        int eliminated_at = sqlite3_column_int(stmt, 4);

        if (rank == 1 && lives > 0) {
            strncpy(winner, pseudo, sizeof(winner) - 1);
        }

        if (!first)
            ok = ok && append_str(&json, &length, &capacity, ",");
        first = false;

        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"rank\":%d,\"pseudo\":\"", rank++);
        ok = ok && append_str(&json, &length, &capacity, buf);
        ok = ok && append_json_string(&json, &length, &capacity, pseudo);

        snprintf(buf, sizeof(buf),
            "\",\"lives\":%d,\"score\":%d,\"correctAnswers\":%d",
            lives, score, correct);
        ok = ok && append_str(&json, &length, &capacity, buf);

        if (lives == 0) {
            snprintf(buf, sizeof(buf),
                ",\"eliminatedAt\":%d", eliminated_at);
            ok = ok && append_str(&json, &length, &capacity, buf);
        }

        ok = ok && append_str(&json, &length, &capacity, "}");
    }

    ok = ok && append_str(&json, &length, &capacity, "],\"winner\":\"");
    ok = ok && append_json_string(&json, &length, &capacity, winner);
    ok = ok && append_str(&json, &length, &capacity, "\"}\n");

    sqlite3_finalize(stmt);

    if (ok) {
        broadcast_to_session(db, session_id, json);
    }

    free(json);
}

void send_session_finished(int session_id, sqlite3 *db) {
    char mode[16] = {0};

    const char *sql = "SELECT mode FROM sessions WHERE id = ?";
    sqlite3_stmt *stmt;

    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt, 1, session_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        strncpy(mode,
            (const char *)sqlite3_column_text(stmt, 0),
            sizeof(mode) - 1);
    }
    sqlite3_finalize(stmt);

    if (strcmp(mode, "solo") == 0) {
        send_session_finished_solo(session_id, db);
    } else {
        send_session_finished_battle(session_id, db);
    }

    sqlite3_exec(db,
        "UPDATE sessions SET status='finished' WHERE id=?",
        NULL, NULL, NULL);
}

static void *question_loop_thread(void *arg) {
    SessionStartArgs *args = (SessionStartArgs *)arg;
    sqlite3 *db = args->db;
    int session_id = args->session_id;

    int nb_questions = args->nb_questions;
    int time_limit = 30; // par défaut
    int current_question = 1; // initial par défaut

    // Récupérer le current_question et le time_limit réel depuis la DB
    const char *sql = "SELECT current_question, time_limit FROM sessions WHERE id = ?";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, session_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            current_question = sqlite3_column_int(stmt, 0);
            time_limit = sqlite3_column_int(stmt, 1);
        }
    }
    sqlite3_finalize(stmt);

    printf("DEBUG question_loop_thread: session_id=%d, starting at question=%d, nb_questions=%d, time_limit=%d\n",
           session_id, current_question, nb_questions, time_limit);

    // Boucle d'envoi des questions suivantes
    for (int i = current_question + 1; i <= nb_questions; i++) {
        sleep(time_limit); // attendre le temps de réponse
        send_question_results(session_id, db);
        send_next_question(db, session_id);
    }
    // send_question_results(session_id, db);
    // sleep(time_limit/2);
    send_session_finished(session_id, db);
    return NULL;
}

// Envoie la première question à tous les joueurs d'une session
static void send_first_question(sqlite3 *db, int session_id, int nb_questions) {
    // Récupérer le time_limit de la session
    const char *sql_time = "SELECT time_limit FROM sessions WHERE id = ?";
    sqlite3_stmt *stmt_time;
    int time_limit = 30; // valeur par défaut
    if (sqlite3_prepare_v2(db, sql_time, -1, &stmt_time, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt_time, 1, session_id);
        if (sqlite3_step(stmt_time) == SQLITE_ROW) {
            time_limit = sqlite3_column_int(stmt_time, 0);
        }
    }
    sqlite3_finalize(stmt_time);
    
    // Récupérer la première question de la session
    const char *sql = 
        "SELECT q.id, q.type, q.enonce, q.answers, q.difficulty, sq.num_question "
        "FROM session_questions sq "
        "JOIN questions q ON sq.question_id = q.id "
        "WHERE sq.session_id = ? AND sq.num_question = 1";
    
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        printf("DEBUG send_first_question: erreur SQL prepare\n");
        return;
    }
    
    sqlite3_bind_int(stmt, 1, session_id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int question_id = sqlite3_column_int(stmt, 0);
        const unsigned char *type = sqlite3_column_text(stmt, 1);
        const unsigned char *enonce = sqlite3_column_text(stmt, 2);
        const unsigned char *answers_json = sqlite3_column_text(stmt, 3);
        const unsigned char *difficulty = sqlite3_column_text(stmt, 4);
        int num_question = sqlite3_column_int(stmt, 5);

        // Sécuriser les valeurs NULL
        if (!type) type = (const unsigned char *)"";
        if (!enonce) enonce = (const unsigned char *)"";
        if (!difficulty) difficulty = (const unsigned char *)"";
        if (!answers_json) answers_json = (const unsigned char *)"[]";

        printf("DEBUG send_first_question: question_id=%d, type=%s, enonce=%s, difficulty=%s, answers=%s\n",
            question_id, type, enonce, difficulty, answers_json);

        // Construire le message JSON
        size_t capacity = 2048;
        size_t length = 0;
        char *json = malloc(capacity);
        if (!json) {
            sqlite3_finalize(stmt);
            printf("DEBUG send_first_question: erreur malloc\n");
            return;
        }
        json[0] = '\0';

        bool ok = true;
        ok = ok && append_str(&json, &length, &capacity, "POST question/new\n{");
        ok = ok && append_str(&json, &length, &capacity, "\"action\":\"question/new\",\"questionNum\":");
        char num_buf[32], total_buf[32];
        snprintf(num_buf, sizeof(num_buf), "%d", num_question);
        snprintf(total_buf, sizeof(total_buf), "%d", nb_questions);
        ok = ok && append_str(&json, &length, &capacity, num_buf);
        ok = ok && append_str(&json, &length, &capacity, ",\"totalQuestions\":");
        ok = ok && append_str(&json, &length, &capacity, total_buf);
        ok = ok && append_str(&json, &length, &capacity, ",\"type\":\"");
        ok = ok && append_json_string(&json, &length, &capacity, type);
        ok = ok && append_str(&json, &length, &capacity, "\",\"difficulty\":\"");
        ok = ok && append_json_string(&json, &length, &capacity, difficulty);
        ok = ok && append_str(&json, &length, &capacity, "\",\"question\":\"");
        ok = ok && append_json_string(&json, &length, &capacity, enonce);
        ok = ok && append_str(&json, &length, &capacity, "\"");

        char time_limit_buf[32];
        snprintf(time_limit_buf, sizeof(time_limit_buf), "%d", time_limit);
        ok = ok && append_str(&json, &length, &capacity, ",\"timeLimit\":");
        ok = ok && append_str(&json, &length, &capacity, time_limit_buf);

        if (type && strcmp((const char *)type, "qcm") == 0) {
            ok = ok && append_str(&json, &length, &capacity, ",\"answers\":");
            ok = ok && append_str(&json, &length, &capacity, (const char *)answers_json);
        }

        ok = ok && append_str(&json, &length, &capacity, "}\n");

        if (ok) {
            printf("DEBUG send_first_question: message complet (%zu bytes)\n%s\n", length, json);
            broadcast_to_session(db, session_id, json);
        } else {
            printf("DEBUG send_first_question: erreur construction JSON\n");
        }
        
        free(json);
    } else {
        printf("DEBUG send_first_question: aucune question trouvée pour session_id=%d\n", session_id);
    }

    sqlite3_finalize(stmt);

    // const char *sql_upd = "UPDATE sessions SET current_question = 2 WHERE id = ?";
    // sqlite3_stmt *stmt_upd;

    // if (sqlite3_prepare_v2(db, sql_upd, -1, &stmt_upd, NULL) == SQLITE_OK) {
    //     sqlite3_bind_int(stmt_upd, 1, session_id);
    //     sqlite3_step(stmt_upd);
    // }
    // sqlite3_finalize(stmt_upd);

}
// Thread qui gère le countdown et l'envoi de la première question
static void *session_start_thread(void *arg) {
    SessionStartArgs *args = (SessionStartArgs *)arg;
    sqlite3 *db = args->db;
    int session_id = args->session_id;
    int nb_questions = args->nb_questions;

    printf("DEBUG session_start_thread: début, session_id=%d, nb_questions=%d\n", session_id, nb_questions);

    // Envoyer le message de démarrage avec countdown à tous les joueurs
    char start_message[256];
    snprintf(start_message, sizeof(start_message),
        "POST session/started\n{\"action\":\"session/started\",\"message\":\"session is starting\",\"countdown\":3}\n");

    printf("DEBUG session_start_thread: diffusion du message de démarrage\n");
    broadcast_to_session(db, session_id, start_message);

    // Attendre 3 secondes (countdown)
    printf("DEBUG session_start_thread: attente de 3 secondes (countdown)...\n");
    sleep(3);

    // Envoyer la première question
    printf("DEBUG session_start_thread: envoi de la première question\n");
    // send_next_question(db, session_id, nb_questions);
    send_next_question(db, session_id);
   
    // Lancer le thread pour les questions suivantes
    SessionStartArgs *args2 = malloc(sizeof(SessionStartArgs));
    args2->db = db;
    args2->session_id = session_id;
    args2->nb_questions = nb_questions;

    pthread_t thread_next;
    pthread_create(&thread_next, NULL, question_loop_thread, args2);
    pthread_detach(thread_next);
    printf("DEBUG session_start_thread: fin du thread\n");
    free(args);
    return NULL;
}

// Gère la requête POST session/start
void handle_session_start(int client_fd, sqlite3 *db) {
    printf("DEBUG handle_session_start: début, client_fd=%d\n", client_fd);
    
    // Récupérer l'ID du joueur à partir du client_fd
    int player_id = get_player_id_by_fd(client_fd);
    printf("DEBUG handle_session_start: player_id=%d\n", player_id);
    if (player_id <= 0) {
        printf("DEBUG handle_session_start: erreur - joueur non authentifié\n");
        send_error_response(client_fd, "session/start", "401", "Player not authenticated");
        return;
    }
    
    // Récupérer la session du joueur qui est en attente (il doit être dans une session en attente)
    const char *sql = 
    "SELECT ps.session_id "
    "FROM players_session ps "
    "JOIN sessions s ON ps.session_id = s.id "
    "WHERE ps.player_id = ? "
    "ORDER BY s.id DESC "     // Trier par ID décroissant => dernière session en premier
    "LIMIT 1";                // Ne garder que la dernière session

    sqlite3_stmt *stmt;
    int session_id = -1;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        printf("DEBUG handle_session_start: erreur SQL prepare\n");
        send_error_response(client_fd, "session/start", "500", "Database error");
        return;
    }
    
    sqlite3_bind_int(stmt, 1, player_id);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        session_id = sqlite3_column_int(stmt, 0);
        printf("DEBUG handle_session_start: session_id trouvée=%d\n", session_id);
    } else {
        printf("DEBUG handle_session_start: aucune session en attente trouvée pour player_id=%d\n", player_id);
    }
    sqlite3_finalize(stmt);
    
    if (session_id < 0) {
        printf("DEBUG handle_session_start: erreur - joueur pas dans une session\n");
        send_error_response(client_fd, "session/start", "404", "Player not in any session");
        return;
    }
    
    // Vérifier que le joueur est le créateur de la session
    // Vérifier dans players_session si ce joueur est le créateur
    const char *sql_check_creator = "SELECT is_creator FROM players_session WHERE player_id = ? AND session_id = ?";
    sqlite3_stmt *stmt_check_creator;
    bool is_creator = false;
    int real_creator_id = -1;
    
    if (sqlite3_prepare_v2(db, sql_check_creator, -1, &stmt_check_creator, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt_check_creator, 1, player_id);
        sqlite3_bind_int(stmt_check_creator, 2, session_id);
        if (sqlite3_step(stmt_check_creator) == SQLITE_ROW) {
            is_creator = (sqlite3_column_int(stmt_check_creator, 0) == 1);
        }
    }
    sqlite3_finalize(stmt_check_creator);
    
    // Récupérer aussi le créateur depuis la table sessions pour debug
    const char *sql_creator = "SELECT created_by FROM sessions WHERE id = ?";
    sqlite3_stmt *stmt_creator;
    
    if (sqlite3_prepare_v2(db, sql_creator, -1, &stmt_creator, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt_creator, 1, session_id);
        if (sqlite3_step(stmt_creator) == SQLITE_ROW) {
            real_creator_id = sqlite3_column_int(stmt_creator, 0);
        }
    }
    sqlite3_finalize(stmt_creator);
    print_authenticated_clients();
    printf("DEBUG handle_session_start: real_creator_id (from sessions)=%d, player_id=%d, is_creator (from players_session)=%d, session_id= %d\n", real_creator_id, player_id, is_creator, session_id);
    
    if (!is_creator) {
        printf("DEBUG handle_session_start: erreur - pas le créateur (créateur réel: %d, joueur actuel: %d, session_id: %d)\n", real_creator_id, player_id, session_id);
        send_error_response(client_fd, "session/start", "403", "Only the creator can start the session");
        return;
    }
    
    // Vérifier que la session est en attente
    const char *sql_status = "SELECT status, nb_questions FROM sessions WHERE id = ?";
    sqlite3_stmt *stmt_status;
    char status[32] = {0};
    int nb_questions = 0;
    
    if (sqlite3_prepare_v2(db, sql_status, -1, &stmt_status, NULL) != SQLITE_OK) {
        printf("DEBUG handle_session_start: erreur SQL prepare status\n");
        send_error_response(client_fd, "session/start", "500", "Database error");
        return;
    }
    
    sqlite3_bind_int(stmt_status, 1, session_id);
    if (sqlite3_step(stmt_status) == SQLITE_ROW) {
        const unsigned char *s = sqlite3_column_text(stmt_status, 0);
        if (s) {
            strncpy(status, (const char *)s, sizeof(status) - 1);
        }
        nb_questions = sqlite3_column_int(stmt_status, 1);
        printf("DEBUG handle_session_start: status=%s, nb_questions=%d\n", status, nb_questions);
    }
    sqlite3_finalize(stmt_status);
    
    if (strcmp(status, "waiting") != 0) {
        printf("DEBUG handle_session_start: erreur - session pas en attente, status=%s\n", status);
        send_error_response(client_fd, "session/start", "400", "Session is not in waiting status");
        return;
    }
    
    // Vérifier qu'il y a au moins 2 joueurs
    int nb_players = get_nb_players(db, session_id);
    printf("DEBUG handle_session_start: nb_players=%d\n", nb_players);
    if (nb_players < 2) {
        printf("DEBUG handle_session_start: erreur - pas assez de joueurs\n");
        send_error_response(client_fd, "session/start", "400", "At least 2 players are required to start");
        return;
    }
    
    // Sélectionner les questions pour la session
    printf("DEBUG handle_session_start: sélection des questions...\n");
    int questions_selected = select_questions_for_session(db, session_id, nb_questions);
    printf("DEBUG handle_session_start: questions_selected=%d sur %d demandées\n", questions_selected, nb_questions);
    
    // Si moins de questions que demandé, on accepte quand même si au moins 1 question
    if (questions_selected < 1) {
        printf("DEBUG handle_session_start: erreur - aucune question sélectionnée\n");
        send_error_response(client_fd, "session/start", "400", "Not enough questions matching criteria");
        return;
    }
    
    // Si moins de questions que demandé, on met à jour le nombre de questions de la session
    if (questions_selected < nb_questions) {
        printf("DEBUG handle_session_start: attention - seulement %d questions disponibles sur %d demandées\n", questions_selected, nb_questions);
        // Mettre à jour le nombre de questions dans la session
        const char *sql_update = "UPDATE sessions SET nb_questions = ? WHERE id = ?";
        sqlite3_stmt *stmt_update;
        if (sqlite3_prepare_v2(db, sql_update, -1, &stmt_update, NULL) == SQLITE_OK) {
            sqlite3_bind_int(stmt_update, 1, questions_selected);
            sqlite3_bind_int(stmt_update, 2, session_id);
            sqlite3_step(stmt_update);
        }
        sqlite3_finalize(stmt_update);
        nb_questions = questions_selected;  // Utiliser le nombre réel de questions
    }
    
    // Mettre à jour le statut de la session à "playing"
    printf("DEBUG handle_session_start: mise à jour du statut...\n");
    if (update_session_status(db, session_id, "playing") != 0) {
        printf("DEBUG handle_session_start: erreur - échec mise à jour statut\n");
        send_error_response(client_fd, "session/start", "500", "Failed to update session status");
        return;
    }
    
    // Envoyer une réponse de succès au créateur
    printf("DEBUG handle_session_start: envoi réponse succès et démarrage thread\n");
    send_json_response(client_fd, "session/start", "200", "Session started successfully");
    
    // Lancer le thread pour gérer le countdown et l'envoi de la première question
    SessionStartArgs *args = malloc(sizeof(SessionStartArgs));
    if (args) {
        args->db = db;
        args->session_id = session_id;
        args->nb_questions = nb_questions;
        
        pthread_t thread;
        if (pthread_create(&thread, NULL, session_start_thread, args) != 0) {
            printf("DEBUG handle_session_start: erreur - échec création thread\n");
            free(args);
            send_error_response(client_fd, "session/start", "500", "Failed to start session thread");
            return;
        }
        pthread_detach(thread);
        printf("DEBUG handle_session_start: thread démarré avec succès\n");
    } else {
        printf("DEBUG handle_session_start: erreur - échec allocation mémoire\n");
    }
}
