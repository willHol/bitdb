#pragma once
#include "hash_map.h"

typedef struct {
	int fd;
	hash_map map;
} bit_db_conn;

int
bit_db_init(const char *pathname);

bit_db_conn *
bit_db_connect(const char *pathname);

int
bit_db_put(bit_db_conn *conn, char *key, void *value, size_t bytes);

int
bit_db_get(bit_db_conn *conn, char *key, void *value);

