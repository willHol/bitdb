#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
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
 * We write blocks of: key_size | key | data_size | data
 * and store the file offset for the data in memory.
 * The offset points to key_size.
 */
int
bit_db_put(bit_db_conn *conn, char *key, void *value, size_t bytes)
{
	off_t off = lseek(conn->fd, 0, SEEK_CUR);
	size_t key_len = strlen(key);

	struct iovec iov[] = {
		{.iov_base = (void *)&key_len, .iov_len = sizeof(size_t)},
		{.iov_base = (void *)key, .iov_len = key_len},
		{.iov_base = (void *)&bytes, .iov_len = sizeof(size_t)},
		{.iov_base = value, .iov_len = bytes}
	};

	if (writev(conn->fd, iov, 4) < 0)
		return -1;

	return hash_map_put(&conn->map, key, &off); 		
}

int
bit_db_get(bit_db_conn *conn, char *key, void *value)
{
	off_t *base_off, data_off;
	size_t key_size;
	char *read_key[key_size];
	size_t data_size;

	hash_map_get(&conn->map, key, &base_off);
	data_off = *base_off + sizeof(size_t);

	/* We need to also read the key from disk, that way
	 * we can return an error if the key is not does not
	 * actually exist at the specified offset
	 */
	struct iovec iov[] {
		{.iov_base = 
	};

	if (pread(conn->fd, &data_size, sizeof(size_t), *base_off) == -1)
		return -1;
	
	if(pread(conn->fd, value, data_size, data_off) == -1)
		return -1;

	return 0;
}

