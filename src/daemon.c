#include <sys/types.h>
#include <sys/wait.h>
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
#include <pthread.h>
#include "error_functions.h"
#include "inet_sockets.h"
#include "sl_list.h"
#include "dl_list.h"
#include "bit_db.h"

#define MAX_SEGMENT_SIZE 128 // Small for testing purposes
#define DIRECTORY "db"
#define NAME_PREFIX "db/bit_db"
#define BUF_SIZE 4096
#define SERVICE "25224" // Port
#define BACKLOG 10 // Number of pending connections
#define NTHREADS 2

/* 
 * Each connection is associated with an open file and
 * a hash table, mapping the key value contents of the file
 * this makes it easy to partition the database across
 * many files
 */

bool volatile run = true;
static dl_list connections;		/* A stack of segment file connections */
static pthread_mutex_t conns_mtx = PTHREAD_MUTEX_INITIALIZER;

static dl_list clients;			/* A queue of client file descriptors */
static pthread_cond_t clients_new = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t clients_mtx = PTHREAD_MUTEX_INITIALIZER;

struct thread_info {		/* Used as an argument to thread_start() */
	pthread_t thread_id;	/* ID returned by pthread_create() */
	int thread_num;		/* Application-defined thread # */
};

static void *handle_request(void *cfd);

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
	return snprintf(NULL, 0, "%lu", (unsigned long)num) - (num < 0);
}

static void
init_connections(void)
{
	size_t segment_count = count_num_segments();
	size_t conns_num_digits = count_digits(segment_count);
	char pathname[strlen(NAME_PREFIX) + conns_num_digits];
	char num_string[conns_num_digits];	
	bit_db_conn *connection;

	for (size_t i = 0; i < segment_count; i++) {
		strcpy(pathname, NAME_PREFIX);
		sprintf(num_string, "%lu", (unsigned long)i);
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

static void
init_data()
{
	if (dl_list_init(&connections, sizeof(bit_db_conn), false) == -1)
                exit(EXIT_FAILURE);
	if (dl_list_init(&clients, sizeof(int), true) == -1)
                exit(EXIT_FAILURE);
}

static void
init_workers(dl_list *worker_threads)
{
	int s;
	pthread_t thread, *stored_thread;

	if (dl_list_init(worker_threads, sizeof(pthread_t), true) == -1)
                exit(EXIT_FAILURE);
	
	for (size_t i = 0; i < NTHREADS; i++) {
		if (dl_list_push(worker_threads, &thread) == -1)
			errExit("dl_list_push()");
		if (dl_list_stack_peek(worker_threads, (void **)&stored_thread) == -1)
			errExit("dl_list_stack_peek()");
		
		s = pthread_create(stored_thread, NULL, handle_request, NULL);
                if (s != 0)
                        errExitEN(s, "pthread_create");
	}
}

static void
destroy_workers(dl_list *worker_threads)
{
	int s;
	pthread_t *thread;
	
	/* Wake up all workers so they can check run variable */
	pthread_cond_broadcast(&clients_new);
	for (size_t i = 0; i < NTHREADS; i++) {
		if (dl_list_dequeue(worker_threads, (void **)&thread) == -1)
			break;
		
		if ((s = pthread_kill(*thread, SIGUSR1)) != 0)
			syslog(LOG_ERR, "Failed to kill thread (%s)", strerror(s));
		if ((s = pthread_join(*thread, NULL)) != 0)
			syslog(LOG_ERR, "Failed to join thread (%s)", strerror(s));
		free(thread);	
	}
	dl_list_destroy(worker_threads);
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
	int *cfd;
	
	printf("[DEBUG] Cleaning up\n");
	for (size_t i = 0; i < connections.num_elems; i++) {
		conn = get_conn(i);
		if (conn != NULL && bit_db_destroy_conn(conn) == -1)
			printf("[DEBUG] Unable to destroy connection #%ld", (long)i);
		if (conn != NULL)
			free(conn);
	}
	dl_list_destroy(&connections);

	/* Close connection to any remaining clients */
	for (size_t i = 0; i < clients.num_elems; i++) {
		dl_list_dequeue(&clients, (void **)&cfd);
		close(*cfd);
	}
	dl_list_destroy(&clients);
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
sig_int_handler(__attribute__((unused))int signum)
{
	run = false;
}

static void
sig_usr_handler(__attribute__((unused))int signum)
{}

static void *
handle_request(__attribute__((unused))void *fd)
{
	int s, t;
	int *cfd = NULL;
	ssize_t num_read = 0;
	char line[BUF_SIZE];
	char *token;

WAIT:
	s = pthread_mutex_lock(&clients_mtx);
	if (s != 0)
		errExitEN(s, "pthread_mutex_lock()");

	s = pthread_cond_wait(&clients_new, &clients_mtx);
	if (s != 0)
		errExitEN(s, "pthread_cond_wait()");

	if (!run) {
		pthread_mutex_unlock(&clients_mtx);
		return NULL;
	}

	if (dl_list_dequeue(&clients, (void **)&cfd) == -1) /* cfd will point to an int sfd */ 
		errExitEN(s, "dl_list_dequeue()");

	s = pthread_mutex_unlock(&clients_mtx);
	if (s != 0)
		errExitEN(s, "pthread_mutex_unlock()");

	while (true) {
		/* Now we can serve the client */
		while (run && ((num_read = read_line(*cfd, line, BUF_SIZE)) > 0)) {
			token = strtok(line, " ");
			if (token == NULL)
				continue;

			printf("TOKEN: %s", token);
		}
		close(*cfd);
		free(cfd);

		if (num_read == -1 && errno == EINTR)
			break;

		s = pthread_mutex_lock(&clients_mtx);
        	if (s != 0)
                	errExitEN(s, "pthread_mutex_lock()");
	
		t = dl_list_dequeue(&clients, (void **)&cfd);

        	s = pthread_mutex_unlock(&clients_mtx);
        	if (s != 0)
                	errExitEN(s, "pthread_mutex_unlock()");

		if (t == 0) {
			/* Got another client to serve */
			continue;
		}
		else if (errno != EINTR) {
			/* No clients left to serve */
			goto WAIT;
		}
		/* Must have been an interrupt */
		break;
	}
	return NULL;
} 

int
main(void)
{
	int lfd, cfd, s;
	struct sigaction sa;
	dl_list worker_threads;		/* A queue for storing worker threads */

	// TODO: becomeDaemon

	init_data();	/* Calls any initialisation functions for global data structures */
	init_connections();		/* Opens fds to segment files and rebuilds tables */
	init_workers(&worker_threads);

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = sig_int_handler;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		printf("[ERROR] Failed to register SIGINT handler\n");
		exit(EXIT_FAILURE);
	}
	sa.sa_handler = sig_usr_handler;
	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
                printf("[ERROR] Failed to register SIGUSR1 handler\n");
                exit(EXIT_FAILURE);
        }

	if ((lfd = inetListen(SERVICE, BACKLOG, NULL)) == -1) {
                syslog(LOG_ERR, "Could not create server socket (%s)", strerror(errno));
                errExit("inetListen()");
        }
        printf("[INFO] Listening on socket: %s\n", SERVICE);
	
	while (run) {
		if ((cfd = accept(lfd, NULL, NULL)) == -1) {
			syslog(LOG_ERR, "Failure in accept(): %s", strerror(errno));
			break;
		}

		/* bigefpfhnfcobdlfbedofhhaibnlghodLock the clients list, enqueue the new cfd, unlock then signal */
		s = pthread_mutex_lock(&clients_mtx);
		if (s != 0)
			break;

		dl_list_enqueue(&clients, &cfd); 

		s = pthread_mutex_unlock(&clients_mtx);
		if (s != 0)
			break;

		s = pthread_cond_signal(&clients_new); /* Wakes a single worker */
                if (s != 0)
                        break;

		/*
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
		*/
		// For testing
		//run = false;
	}
	printf("\n");

	close(lfd);
	persist_tables();
	destroy_workers(&worker_threads);
	cleanup();
	
	printf("[INFO] Exiting daemon\n");
	exit(EXIT_SUCCESS);
}
