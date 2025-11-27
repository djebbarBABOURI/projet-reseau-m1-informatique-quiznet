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
        if (creator_id == player_id) {
            send_error_response(client_fd, "session/join", "403", "Creator cannot join their own session");
            return;
        }

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

    send_error_response(client_fd, "unknown", "404", "endpoint not implemented");
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
