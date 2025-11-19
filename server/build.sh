#!/bin/sh

set -xe

gcc main.c db_utils.c sqlite/sql.c sqlite/libsqlite3.a -I./sqlite -lpthread -o main