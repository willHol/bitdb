#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include <regex.h>
#include "sl_list.h"
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
static bit_db_conn connections[MAX_NUM_SEGMENTS];
static size_t num_segments;

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
	return count;
}

static int
count_digits(int num)
{
	return snprintf(NULL, 0, "%d", num) - (num < 0);
}

static void
init_connections(void)
{
	num_segments = count_num_segments();
	int result;
	int conns_num_digits = count_digits(num_segments);
	char pathname[strlen(NAME_PREFIX) + conns_num_digits];
	char num_string[conns_num_digits];	
	size_t segment_count;

	segment_count = (num_segments > 0) ? num_segments : 1;
	for (size_t i = 0; i < segment_count; i++) {
		strcpy(pathname, NAME_PREFIX);
		sprintf(num_string, "%d", i);
		strcat(pathname, num_string);
		
		if (bit_db_connect(&connections[i], pathname) == -1) {
			if ((errno == EMAGICSEQ || errno == ENOENT) && bit_db_init(pathname) != 0) {
				/*
				 * Segment doesn't exist or has an invalid magic sequence.
				 * Unable to create a working segment file.
				 */
				printf("[ERROR] Failed to create segment file\nERRNO: %ld\n", (long)errno);
				exit(EXIT_FAILURE);
			}
			if (bit_db_connect(&connections[i], pathname) == -1)
				exit(EXIT_FAILURE);
			printf("[INFO] Created segment file \"%s\"\n", pathname);
		}
		printf("[INFO] Opened connection to segment file \"%s\"\n", pathname);
	}
}

static void
cleanup(void)
{
	for (size_t i = 0; i < num_segments; i++) {
		if (bit_db_destroy_conn(&connections[i]) == -1)
			printf("[DEBUG] Unable to destroy connection #%ld", (long)i);
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
	/* Receive queries and respond to them */
	for (;run;) {
	
	}
	char value[5];	
	int result;
	if (bit_db_get(&connections[0], "key", value) == -1)
		printf("file sucks sock!\n");
	else
		printf("GOT: %s\n", value);
	
	result = bit_db_put(&connections[0], "key", "value", 6);
	result = bit_db_put(&connections[0], "key2", "value2", 7);


	for (size_t i = 0; i < num_segments; i++) {
		if (bit_db_persist_table(&connections[i]) == -1)
                	printf("[INFO] Failed to persist connection\n");
        	else
                	printf("[INFO] Successfully persisted connection\n");
	}

	printf("[DEBUG] Cleaning up\n");
	cleanup();
	
	printf("[INFO] Exiting daemon\n");
	exit(EXIT_SUCCESS);
}
