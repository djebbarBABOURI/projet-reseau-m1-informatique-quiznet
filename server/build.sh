#!/bin/sh

set -xe
gcc main.c db_utils.c sqlite/sql.c sqlite/libsqlite3.a -I./sqlite -lpthread -o main
# gcc -Wall -Wextra -O2 \
#     main.c \
#     db_utils.c \
#     sqlite/sql.c \
#     -I./sqlite \
#     -lsqlite3 \
#     -lpthread \
#     -ldl \
#     -o main