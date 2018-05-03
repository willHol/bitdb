#pragma once
#include <limits.h>
#include <sys/types.h>
#include "hash_map.h"

#define EKEYNOTFOUND -1
#define EKEYNOTFOUNDDISK -2
#define EMAGICSEQ -3
#define ECHECKSUM -4

typedef struct {
	int fd;
	char pathname[_POSIX_PATH_MAX];
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
bit_db_connect_full(bit_db_conn *conn);

int
bit_db_put(bit_db_conn *conn, char *key, void *value, size_t bytes);

size_t
bit_db_get(bit_db_conn *conn, char *key, void *value);

int
bit_db_keys(bit_db_conn *conn, dl_list *list);

int
bit_db_persist_table(bit_db_conn *conn);

int
bit_db_retrieve_table(bit_db_conn *conn);

