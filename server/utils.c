#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/socket.h>

//
void send_json_response(int fd, const char *action, const char *status, const char *message)
{
    char response[512];
    snprintf(response, sizeof(response),
             "{\"action\":\"%s\",\"statut\":\"%s\",\"message\":\"%s\"}\n",
             action, status, message);
    send(fd, response, strlen(response), 0);
}
//
void send_register_success(int fd)
{
    send_json_response(fd, "player/register", "201", "player registered successfully");
}

//
void send_login_success(int fd)
{
    send_json_response(fd, "player/login", "200", "login successful");
}

//
void send_error_response(int fd, const char *action, const char *status, const char *message)
{
    send_json_response(fd, action, status, message);
}

// ----------------------------------------
// Fonctions d'extraction de données JSON
// ----------------------------------------
// Fonction pour extraire un entier JSON
//
bool extract_json_int(const char *json, const char *key, int *out)
{
    if (!json || !key || !out) return false;
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *key_pos = strstr(json, pattern);
    if (!key_pos) return false;
    const char *colon = strchr(key_pos + strlen(pattern), ':');
    if (!colon) return false;
    colon++;
    while (*colon && isspace((unsigned char)*colon)) colon++;
    char *endptr;
    long val = strtol(colon, &endptr, 10);
    if (endptr == colon) return false;
    *out = (int)val;
    return true;
}

// Fonction pour extraire un tableau d'entiers JSON (ex: [1,2,3])
//
bool extract_json_int_array(const char *json, const char *key, int **out, size_t *count)
{
    if (!json || !key || !out || !count) return false;
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *key_pos = strstr(json, pattern);
    if (!key_pos) return false;
    const char *colon = strchr(key_pos + strlen(pattern), ':');
    if (!colon) return false;
    colon++;
    while (*colon && isspace((unsigned char)*colon)) colon++;
    if (*colon != '[') return false;
    colon++;
    
    // Compter les éléments
    *count = 0;
    const char *p = colon;
    while (*p && *p != ']') {
        if (isdigit((unsigned char)*p)) {
            (*count)++;
            while (*p && (isdigit((unsigned char)*p) || *p == ',' || isspace((unsigned char)*p))) {
                if (*p == ',') break;
                p++;
            }
        }
        if (*p == ',') p++;
        while (*p && isspace((unsigned char)*p)) p++;
    }
    
    if (*count == 0) return false;
    
    // Allouer et remplir le tableau
    *out = malloc(*count * sizeof(int));
    if (!*out) return false;
    
    p = colon;
    size_t idx = 0;
    while (*p && *p != ']' && idx < *count) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (!isdigit((unsigned char)*p)) break;
        char *endptr;
        long val = strtol(p, &endptr, 10);
        (*out)[idx++] = (int)val;
        p = endptr;
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
    }
    
    return true;
}

// ----------------------------------------
// Fonctions de manipulation de chaînes dynamiques
// ----------------------------------------
//
bool append_str(char **buffer, size_t *length, size_t *capacity, const char *text)
{
    size_t to_add = strlen(text);
    if (!ensure_capacity(buffer, capacity, *length + to_add + 1)) {
        return false;
    }
    memcpy(*buffer + *length, text, to_add);
    *length += to_add;
    (*buffer)[*length] = '\0';
    return true;
}

//
bool append_char(char **buffer, size_t *length, size_t *capacity, char c)
{
    if (!ensure_capacity(buffer, capacity, *length + 2)) {
        return false;
    }
    (*buffer)[(*length)++] = c;
    (*buffer)[*length] = '\0';
    return true;
}

//
bool append_json_string(char **buffer, size_t *length, size_t *capacity, const unsigned char *text)
{
    if (!text) {
        return true;
    }
    const unsigned char *p = text;
    while (*p) {
        unsigned char c = *p++;
        switch (c) {
            case '\"':
            case '\\':
                if (!append_char(buffer, length, capacity, '\\') ||
                    !append_char(buffer, length, capacity, (char)c)) {
                    return false;
                }
                break;
            case '\b':
                if (!append_str(buffer, length, capacity, "\\b")) return false;
                break;
            case '\f':
                if (!append_str(buffer, length, capacity, "\\f")) return false;
                break;
            case '\n':
                if (!append_str(buffer, length, capacity, "\\n")) return false;
                break;
            case '\r':
                if (!append_str(buffer, length, capacity, "\\r")) return false;
                break;
            case '\t':
                if (!append_str(buffer, length, capacity, "\\t")) return false;
                break;
            default:
                if (c < 0x20) {
                    char esc[7];
                    snprintf(esc, sizeof(esc), "\\u%04x", c);
                    if (!append_str(buffer, length, capacity, esc)) return false;
                } else {
                    if (!append_char(buffer, length, capacity, (char)c)) return false;
                }
                break;
        }
    }
    return true;
}


//
bool ensure_capacity(char **buffer, size_t *capacity, size_t required)
{
    if (*capacity >= required) {
        return true;
    }
    size_t new_capacity = *capacity;
    while (new_capacity < required) {
        new_capacity *= 2;
    }
    char *tmp = realloc(*buffer, new_capacity);
    if (!tmp) {
        return false;
    }
    *buffer = tmp;
    *capacity = new_capacity;
    return true;
}

bool extract_json_raw(const char *json, const char *key, char *out, size_t outSize)
{
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(json, pattern);
    if (!pos) return false;

    pos = strchr(pos, ':');
    if (!pos) return false;
    pos++;

    // on copie jusqu'à "," ou "}" ou fin
    const char *end = pos;
    while (*end && *end != ',' && *end != '}' && *end != '\n') end++;

    size_t len = end - pos;
    if (len >= outSize) return false;

    strncpy(out, pos, len);
    out[len] = '\0';

    // Trim espaces
    trim(out);

    return true;
}


char *trim(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

bool extract_json_double(const char *json, const char *key, double *out_value) {
    // très simple pour test, on suppose {"key":value}
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    char *pos = strstr(json, pattern);
    if(!pos) return false;
    pos += strlen(pattern);
    *out_value = atof(pos);
    return true;
}
