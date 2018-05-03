#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include <regex.h>
#include "error_functions.h"
#include "sl_list.h"
#include "dl_list.h"
#include "bit_db.h"

#define MAX_SEGMENT_SIZE 64
#define MAX_NUM_SEGMENTS 128
#define DIRECTORY "db"
#define NAME_PREFIX "db/bit_db"

/* 
 * Each connection is associated with an open file and
 * a hash table, mapping the key value contents of the file
 * this makes it easy to partition the database across
 * many files
 */

bool volatile run = true;
static dl_list connections;

static size_t
count_num_segments()
{
	size_t count = 0;
	struct dirent *dir;
	DIR *dirp = opendir(DIRECTORY);
	regex_t regex;

	/* bit_db001 but not bit_db001.tb */
	if (regcomp(&regex, "^bit_db[0-9]+$", REG_EXTENDED) != 0)
		return 0;

	while ((dir = readdir(dirp)) != NULL)
		if (regexec(&regex, dir->d_name, 0, NULL, 0) == 0)
			count ++;
	
	regfree(&regex);	
	closedir(dirp);
	return (count > 0) ? count : 1;
}

static size_t
count_digits(size_t num)
{
	return snprintf(NULL, 0, "%d", num) - (num < 0);
}

static void
init_connections(void)
{
	size_t segment_count = count_num_segments();
	size_t conns_num_digits = count_digits(segment_count);
	char pathname[strlen(NAME_PREFIX) + conns_num_digits];
	char num_string[conns_num_digits];	
	bit_db_conn *connection;

	if (dl_list_init(&connections, sizeof(bit_db_conn *)) == -1)
		exit(EXIT_FAILURE);

	for (size_t i = 0; i < segment_count; i++) {
		strcpy(pathname, NAME_PREFIX);
		sprintf(num_string, "%d", i);
		strcat(pathname, num_string);
		
		connection = calloc(1, sizeof(bit_db_conn));
		if (connection == NULL)
			errExit("calloc");

		if (bit_db_connect(connection, pathname) == -1) {
			if ((errno == EMAGICSEQ || errno == ENOENT) && bit_db_init(pathname) != 0) {
				/*
				 * Segment doesn't exist or has an invalid magic sequence.
				 * Unable to create a working segment file.
				 */
				printf("[ERROR] Failed to create segment file \"%s\"\n", pathname);
				free(connection);
				exit(EXIT_FAILURE);
			}
			if (bit_db_connect(connection, pathname) == -1) {
				printf("[ERROR] Failed to open connection to segment file \"%s\"", pathname);
				free(connection);
				exit(EXIT_FAILURE);
			}
			printf("[INFO] Created segment file \"%s\"\n", pathname);
		}
		if (dl_list_add(&connections, i, connection) == -1)
			errExit("dl_list_set()");

		printf("[INFO] Opened connection to segment file \"%s\"\n", pathname);
	}
}

static bit_db_conn *
get_conn(size_t i)
{
        bit_db_conn *out;
        if (dl_list_get(&connections, i, (void **)&out) == -1)
                return NULL;
        return out;
}


static void
cleanup(void)
{
	bit_db_conn *conn;
	
	printf("[DEBUG] Cleaning up\n");
	for (size_t i = 0; i < connections.num_elems; i++) {
		conn = get_conn(i);
		if (conn != NULL && bit_db_destroy_conn(conn) == -1)
			printf("[DEBUG] Unable to destroy connection #%ld", (long)i);
		free(conn);
	}
}

static void
persist_tables(void)
{
	for (size_t i = 0; i < connections.num_elems; i++) {
                if (bit_db_persist_table(get_conn(i)) == -1)
                        printf("[INFO] Failed to persist connection\n");
                else
                        printf("[INFO] Successfully persisted connection\n");
        }
}

static void
sig_int_handler(int dummy)
{
	run = false;
}

int
main(int argc, char *argv[])
{
	init_connections();	
	signal(SIGINT, sig_int_handler);

	// TODO: becomeDaemon
	
	printf("[INFO] Entering loop\n");
	while (run) {
		/* Receive queries and respond to them */	
	}
	printf("\n");

	persist_tables();
	cleanup();
	
	printf("[INFO] Exiting daemon\n");
	exit(EXIT_SUCCESS);
}
