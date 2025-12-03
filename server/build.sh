#!/bin/sh

set -xe
# gcc main.c db_utils.c sqlite/sql.c sqlite/libsqlite3.a -I./sqlite -lpthread -o main
# gcc main.c utils.c questions.c clients.c udp.c db_utils.c handlers.c server.c sha256.c sqlite/sql.c -I./sqlite -lsqlitئe3 -lpthread -o main
gcc main.c utils.c questions.c clients.c udp.c db_utils.c handlers.c server.c sha256.c sqlite/sql.c -I./sqlite -lpthread -lsqlite3 -o main

# gcc -Wall -Wextra -O2 \
#     main.c \
#     db_utils.c \
#     sqlite/sql.c \
#     -I./sqlite \
#     -lsqlite3 \
#     -lpthread \
#     -ldl \
#     -o main