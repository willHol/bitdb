#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "hash_map.h"
#include "bit_db.h"

static const char default_name[] = "bit_db";

int
bit_db_init(const char *pathname)
{
	int fd;
	int flags = O_WRONLY | O_CREAT | O_TRUNC;
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
	
	pathname = pathname != NULL ? pathname : default_name;
	if ((fd = open(pathname, flags, mode)) == -1)
		return -1;
	if (close(fd) == -1)
		return -1;
	return 0;
}

bit_db_conn *
bit_db_connect(const char *pathname)
{
	bit_db_conn *conn = malloc(sizeof(*conn));
	int flags = O_RDWR | O_APPEND;
	
	pathname = pathname != NULL ? pathname : default_name;
	if ((conn->fd = open(pathname, flags)) == -1)
		return NULL;
	
	hash_map_init(&conn->map);
	return conn;
}

/*
 * We write blocks of file_size | key | data
 * and store the file offset for the data in memory.
 */
int
bit_db_put(bit_db_conn *conn, const char *key, void *value, size_t bytes)
{
		
}

