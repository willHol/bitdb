#pragma once
#include "hash_map.h"

#define EKEYNOTFOUND 1
#define EKEYNOTFOUNDDISK 2

typedef struct {
	int fd;
	hash_map map;
} bit_db_conn;

int
bit_db_init(const char *pathname);

int
bit_db_destroy_conn(bit_db_conn *conn);

int
bit_db_destroy(const char *pathname);

int
bit_db_connect(bit_db_conn *conn, const char *pathname);

int
bit_db_put(bit_db_conn *conn, char *key, void *value, size_t bytes);

int
bit_db_get(bit_db_conn *conn, char *key, void *value);

