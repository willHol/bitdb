#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <dirent.h>
#include <string.h>
#include <regex.h>
#include "error_functions.h"
#include "inet_sockets.h"
#include "sl_list.h"
#include "dl_list.h"
#include "bit_db.h"

#define MAX_SEGMENT_SIZE 128 // Small for testing purposes
#define DIRECTORY "db"
#define NAME_PREFIX "db/bit_db"
#define BUF_SIZE 4096
#define SERVICE "65528" // Port
#define BACKLOG 10 // Number of pending connections

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
		if (dl_list_enqueue(&connections, connection) == -1)
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
		if (conn != NULL)
			free(conn);
	}
	dl_list_destroy(&connections);
}

static void
persist_tables(void)
{
	bit_db_conn *conn;

	for (size_t i = 0; i < connections.num_elems; i++) {
           	conn = get_conn(i);     
		if (conn == NULL || bit_db_persist_table(conn) == -1)
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
	int lfd, cfd;
	size_t num_read;
	char buf[BUF_SIZE];
	char value[_POSIX_NAME_MAX];
	char pathname[_POSIX_NAME_MAX];
	char num[_POSIX_NAME_MAX];
	bit_db_conn *connection;
	
	init_connections();	

	if ((lfd = inetListen(SERVICE, BACKLOG, NULL)) == -1) {
		syslog(LOG_ERR, "Could not create server socket (%s)", strerror(errno));
		errExit("inetListen()");
	}
	printf("[INFO] Listening on socket: %s\n", SERVICE);

	// TODO: becomeDaemon

	// Just wait for single connection for now
	if ((lfd = accept(lfd, NULL, NULL)) == -1) {
		syslog(LOG_ERR, "Failure in accept(): %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
	printf("[INFO] Client connected\n");

	//signal(SIGINT, sig_int_handler);
	printf("[INFO] Entering loop\n");
	while (run) {
		num_read = 0;
		read_line(lfd, buf, BUF_SIZE);

		printf("BYTES: %s\n", buf);

		// The most recently opened connection
		if (dl_list_peek(&connections, (void **)&connection) == -1)
			break;
		if (connection == NULL) {
			printf("[ERROR] No segments are open\n");
			break;
		}
		// Check if segment is full
		if (bit_db_connect_full(connection)) {
			strcpy(pathname, NAME_PREFIX);
			sprintf(num, "%ld", (long)connections.num_elems);
			strcat(pathname, num);

			connection = malloc(sizeof(bit_db_conn));

			if (bit_db_init(pathname) == -1) {
				printf("[ERROR] Failed to create segment file \"%s\"\n", pathname);
				break;
			}
			if (bit_db_connect(connection, pathname) == -1) {
				printf("[ERROR] Failed to open connection to segment file \"%s\"", pathname);
				break;
			}
			if (dl_list_push(&connections, connection) == -1) {
				break;
			}
			printf("[INFO] Created new segment file.");
		}

		// For testing
		// run = false;
	}
	printf("\n");

	persist_tables();
	cleanup();
	
	printf("[INFO] Exiting daemon\n");
	exit(EXIT_SUCCESS);
}
