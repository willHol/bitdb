#pragma once
#include "hash_map.h"
#include <limits.h>
#include <sys/types.h>

#define EKEYNOTFOUND -1
#define EKEYNOTFOUNDDISK -2
#define EMAGICSEQ -3
#define ECHECKSUM -4

typedef struct {
    int fd;
    char pathname[_POSIX_PATH_MAX];
    /*
     * Concurrent read and writes are allowable as data is never
     * overwritten, however we must ensure that the file is not
     * deleted while we are reading from it
     */
    pthread_mutex_t delete_mtx;
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

ssize_t
bit_db_get(bit_db_conn *conn, char *key, void **value);

int
bit_db_keys(bit_db_conn *conn, dl_list *list);

int
bit_db_persist_table(bit_db_conn *conn);

int
bit_db_retrieve_table(bit_db_conn *conn);
