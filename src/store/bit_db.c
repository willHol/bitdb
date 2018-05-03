#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "error_functions.h"
#include "sha256.h"
#include "dl_list.h"
#include "hash_map.h"
#include "bit_db.h"

static const unsigned long magic_seq = 0x123FFABC;
static const char default_name[] = "bit_db";

int
bit_db_init(const char *pathname)
{
	int fd;
	int flags = O_WRONLY | O_CREAT | O_TRUNC;
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
	
	pathname = pathname != NULL ? pathname : default_name;
	if ((fd = open(pathname, flags, mode)) == -1)
		errExit("open() %s", pathname);
	if (write(fd, &magic_seq, sizeof(magic_seq)) != sizeof(magic_seq))
		errExit("write() %s", pathname);
	if (close(fd) == -1) {
		errMsg("close() %s", pathname);
	}
	return 0;
}

int
bit_db_destroy(const char *pathname)
{
	int fd;
        int flags = O_WRONLY | O_CREAT | O_TRUNC;
        mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

        pathname = pathname != NULL ? pathname : default_name;
        if ((fd = open(pathname, flags, mode)) == -1)
                return -1;
        if (unlink(pathname) == -1)
		return -1;
	if (close(fd) == -1)
                return -1;
        return 0;
}

int
bit_db_destroy_conn(bit_db_conn *conn)
{
	return hash_map_destroy(&conn->map);
}

int
bit_db_connect(bit_db_conn *conn, const char *pathname)
{
	int status = 0;
	int flags = O_RDWR | O_APPEND;
	unsigned long read_magic_seq;
	char table_pathname[strlen(pathname) + 3];
	FILE *fp;

	pathname = pathname != NULL ? pathname : default_name;
	if ((conn->fd = open(pathname, flags)) == -1) {
		errExit("open() %s", pathname);
	}
	if (read(conn->fd, &read_magic_seq, sizeof(unsigned long)) == -1) {
		errExit("read() %s", pathname);
	}
	if (read_magic_seq != magic_seq) {
		errMsg("%s", pathname);
		errno = EMAGICSEQ;
		return -1;
	}

	strcpy(conn->pathname, pathname);
	status = bit_db_retrieve_table(conn);

	return (status == 0) ? 0 : hash_map_init(&conn->map);
}

/*
 * We write blocks of: key_size | key | data_size | data
 * and store the file offset for the data in memory.
 * The offset points to key_size.
 */
int
bit_db_put(bit_db_conn *conn, char *key, void *value, size_t bytes)
{
	off_t off = lseek(conn->fd, 0, SEEK_END);
	size_t key_len = strlen(key) + 1;

	struct iovec iov[] = {
		{.iov_base = (void *)&key_len, .iov_len = sizeof(size_t)},
		{.iov_base = (void *)key, .iov_len = key_len},
		{.iov_base = (void *)&bytes, .iov_len = sizeof(size_t)},
		{.iov_base = value, .iov_len = bytes}
	};

	if (writev(conn->fd, iov, 4) < 0) {
		errMsg("writev() %s", conn->pathname);
		return -1;
	}

	return hash_map_put(&conn->map, key, &off); 		
}

int
bit_db_get(bit_db_conn *conn, char *key, void *value)
{
	off_t *base_off, data_off;
	size_t key_size = strlen(key) + 1;
	char read_key[key_size - 1];
	size_t data_size;

	hash_map_get(&conn->map, key, &base_off);
	if (base_off == NULL) {
		errno = EKEYNOTFOUND;
		return -1;
	}
	
	data_off = *base_off + 2*sizeof(size_t) + key_size;

	/* We need to also read the key from disk, that way
	 * we can return an error if the key is not does not
	 * actually exist at the specified offset
	 */
	struct iovec iov[] = {
		{.iov_base = read_key, .iov_len = key_size},
		{.iov_base = &data_size, .iov_len = sizeof(size_t)}	
	};

	/* Read the key and data size */
	if (preadv(conn->fd, iov, 2, *base_off+sizeof(size_t)) == -1) {
		errMsg("preadv() %s", conn->pathname);
		return -1;
	}

	if (strcmp(key, read_key) != 0) {
		errno = EKEYNOTFOUND;
		return -1;
	}
	
	/* Read the data */
	if(pread(conn->fd, value, data_size, data_off) == -1) {
		errMsg("pread() %s", conn->pathname);
		return -1;
	}

	return 0;
}

int
bit_db_keys(bit_db_conn *conn, dl_list *list)
{
	if (dl_list_init(list, sizeof(char *)) == -1)
		return -1;

	if (hash_map_keys(&conn->map, list) == -1) {
		return -1;
	}

	return 0;
}

#define BUF_SIZE 1024
static void
hash_file(FILE *fp, BYTE hash[])
{
	SHA256_CTX ctx;
        const BYTE buf[BUF_SIZE];
        size_t bytes;

	sha256_init(&ctx);
        while((bytes = fread((void *)buf, 1, BUF_SIZE, fp)) != 0)
                sha256_update(&ctx, buf, BUF_SIZE);
        sha256_final(&ctx, hash);
}

/* 
 * Save the hash table to a appropriately named file
 * Append a check sum so we can verify the validity
 * upon loading.
 */
int
bit_db_persist_table(bit_db_conn *conn)
{
	FILE *tb;
	char pathname[_POSIX_PATH_MAX];
	BYTE hash[SHA256_BLOCK_SIZE];

	strcpy(pathname, conn->pathname);
	strcat(pathname, ".tb");

	if((tb = fopen(pathname, "wr")) == NULL) {
		errMsg("fopen() %s", pathname);
		return -1;
	}

	if (hash_map_write(tb, &conn->map) == -1)
		return -1;

	/* Attach a checksum */
	// TODO: Optimise so it's not reading the file back
	hash_file(tb, hash);

	if (fwrite(hash, 1, SHA256_BLOCK_SIZE, tb) == -1) {
		errMsg("fwrite() %s", pathname);
		return -1;
	}
	
	if (fclose(tb) == -1) {
		errMsg("fclose() %s", pathname);
		return -1;
	}

	return 0;
}

int
bit_db_retrieve_table(bit_db_conn *conn)
{
	int status = 0;
	FILE *fp;
	char pathname[_POSIX_PATH_MAX];
	BYTE read_hash[SHA256_BLOCK_SIZE];
	BYTE attached_hash[SHA256_BLOCK_SIZE];

	strcpy(pathname, conn->pathname);
	strcat(pathname, ".tb");

	if ((fp = fopen(pathname, "r")) == NULL) {
		errMsg("fopen() %s", pathname);
		status = -1;
		goto CLEANUP;
	}

	if (hash_map_read(fp, &conn->map) == -1) {
		status = -1;
		goto CLEANUP;
	}
	
	if (fseek(fp, -SHA256_BLOCK_SIZE, SEEK_END) == -1) {
		errMsg("fseek() %s", pathname);
		status = -1;
		goto CLEANUP;
	}
	if (fread(attached_hash, 1, SHA256_BLOCK_SIZE, fp) == -1) {
		errMsg("fread() %s", pathname);
		status = -1;
		goto CLEANUP;
	}

	hash_file(fp, read_hash);
        if (memcmp(read_hash, attached_hash, SHA256_BLOCK_SIZE) != 0) {
                errno = ECHECKSUM;
		status = -1;
		goto CLEANUP;
	}

CLEANUP:
	if (fp != NULL && fclose(fp) == -1) {
		errMsg("fclose() %s", pathname);
		return -1;
	}
	else {
		return status;

	}
}


