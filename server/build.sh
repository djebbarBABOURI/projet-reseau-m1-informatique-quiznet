#!/bin/sh

set -xe

gcc -Wall -Wextra -O2 \
    main.c \
    db_utils.c \
    sqlite/sql.c \
    -I./sqlite \
    -lsqlite3 \
    -lpthread \
    -ldl \
    -o main