#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "bit_db.h"

#define NUM_CONNECTIONS 1
#define NAME_PREFIX "bit_db"

/* 
 * Each connection is associated with an open file and
 * a hash table, mapping the key value contents of the file
 * this makes it easy to partition the database across
 * many files
 */
static bit_db_conn connections[NUM_CONNECTIONS];

static int
count_digits(int num)
{
	return snprintf(NULL, 0, "%d", num) - (num < 0);
}

static void
init_connections(void)
{
	int result;
	int conns_num_digits = count_digits(NUM_CONNECTIONS);
	char pathname[strlen(NAME_PREFIX) + conns_num_digits];
	char num_string[conns_num_digits];

	for (size_t i = 0; i < NUM_CONNECTIONS; i++) {
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
			printf("[INFO] Created segment file %s\n", pathname);
		}
		printf("[INFO] Opened connection to segment file %s\n", pathname);
	}
}

int
main(int argc, char *argv[])
{
	init_connections();	
	
	printf("[INFO] Successfully exiting daemon\n");
	exit(EXIT_SUCCESS);
}
