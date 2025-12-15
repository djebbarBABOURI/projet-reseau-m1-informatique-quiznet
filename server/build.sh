#!/bin/sh

set -xe

# Compilation du serveur QuizNet en liant la librairie SQLite statique fournie
# On utilise directement sqlite/libsqlite3.a (pas besoin de -lsqlite3 système)
gcc main.c utils.c questions.c clients.c udp.c db_utils.c handlers.c server.c sha256.c \
    sqlite/sql.c \
    sqlite/libsqlite3.a \
    -I./sqlite \
    -lpthread \
    -ldl \
    -o main
