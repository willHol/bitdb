#include "bit_db.h"
#include "dl_list.h"
#include "error_functions.h"
#include "inet_sockets.h"
#include "sl_list.h"
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <regex.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#define MAX_SEGMENT_SIZE 128
#define DIRECTORY "db"
#define NAME_PREFIX "db/bit_db"
#define BUF_SIZE 4096
#define SERVICE "25224"
#define BACKLOG 10
#define NTHREADS 2

/******************************************************/

bool volatile run = true;

static dl_list connections;
static pthread_mutex_t conns_mtx = PTHREAD_MUTEX_INITIALIZER;

static dl_list clients;
static pthread_cond_t clients_new = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t clients_mtx = PTHREAD_MUTEX_INITIALIZER;

static dl_list workers;

/******************************************************/

/******************************************************/

static void*
handle_request(void* cfd);

static void
init_data(void);
static void
open_connections(void);
static void
start_workers(void);

static void
destroy_data(void);
static void
close_connections(void);
static void
stop_workers(void);
static void
persist_tables(void);

static void
sig_int_handler(int signum);
static void
sig_usr_handler(int signum);

static size_t
count_num_segments(void);
static size_t
count_digits(size_t num);

/******************************************************/

int
main(void)
{
    int lfd, cfd, s;
    struct sigaction sa;

    // TODO: becomeDaemon

    init_data();
    open_connections();
    start_workers();

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
                        printf("[ERROR] Failed to create segment file \"%s\"\n",
        pathname); break;
                }
                if (bit_db_connect(connection, pathname) == -1) {
                        printf("[ERROR] Failed to open connection to segment
        file
        \"%s\"", pathname); break;
                }
                if (dl_list_push(&connections, connection) == -1) {
                        break;
                }
                printf("[INFO] Created new segment file.");
        }
        */
        // For testing
        // run = false;
    }
    printf("\n");
    close(lfd);

    stop_workers();
    persist_tables();
    close_connections();
    destroy_data();

    printf("[INFO] Exiting daemon\n");
    exit(EXIT_SUCCESS);
}

/*
 * Called on thread initialisation
 */
static void*
handle_request(__attribute__((unused)) void* fd)
{
    int s, t;
    int* cfd = NULL;
    ssize_t num_read = 0;
    char line[BUF_SIZE];
    char* token;

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

    if (dl_list_dequeue(&clients, (void**)&cfd) ==
        -1) /* cfd will point to an int sfd */
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

        t = dl_list_dequeue(&clients, (void**)&cfd);

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

/*
 * Initialises global data structures
 */
static void
init_data(void)
{
    if (dl_list_init(&connections, sizeof(bit_db_conn), true) == -1)
        exit(EXIT_FAILURE);
    if (dl_list_init(&clients, sizeof(int), true) == -1)
        exit(EXIT_FAILURE);
    if (dl_list_init(&workers, sizeof(pthread_t), true) == -1)
        exit(EXIT_FAILURE);
}

/*
 * Initialises segment file connections
 */
static void
open_connections(void)
{
    size_t segment_count = count_num_segments();
    size_t conns_num_digits = count_digits(segment_count);
    char pathname[strlen(NAME_PREFIX) + conns_num_digits];
    char num_string[conns_num_digits];
    bit_db_conn connection;

    for (size_t i = 0; i < segment_count; i++) {
        strcpy(pathname, NAME_PREFIX);
        sprintf(num_string, "%lu", (unsigned long)i);
        strcat(pathname, num_string);

        if (bit_db_connect(&connection, pathname) == -1) {
            if ((errno == EMAGICSEQ || errno == ENOENT) &&
                bit_db_init(pathname) != 0) {
                /*
                 * Segment doesn't exist or has an invalid magic sequence.
                 * Unable to create a working segment file.
                 */
                printf("[ERROR] Failed to create segment file \"%s\"\n",
                       pathname);
                exit(EXIT_FAILURE);
            }
            if (bit_db_connect(&connection, pathname) == -1) {
                printf(
                  "[ERROR] Failed to open connection to segment file \"%s\"",
                  pathname);
                exit(EXIT_FAILURE);
            }
            printf("[INFO] Created segment file \"%s\"\n", pathname);
        }
        if (dl_list_enqueue(&connections, &connection) == -1)
            errExit("dl_list_set()");

        printf("[INFO] Opened connection to segment file \"%s\"\n", pathname);
    }
}

/*
 * Initialises worker threads
 */
static void
start_workers(void)
{
    int s;
    pthread_t thread, *stored_thread;

    for (size_t i = 0; i < NTHREADS; i++) {
        if (dl_list_push(&workers, &thread) == -1)
            errExit("dl_list_push()");
        if (dl_list_stack_peek(&workers, (void**)&stored_thread) == -1)
            errExit("dl_list_stack_peek()");

        s = pthread_create(stored_thread, NULL, handle_request, NULL);
        if (s != 0)
            errExitEN(s, "pthread_create");
    }
}

/*
 * Deallocates memory from global data structures
 */
static void
destroy_data(void)
{
    int* cfd;

    /* Close connection to any remaining clients */
    for (size_t i = 0; i < clients.num_elems; i++) {
        if (dl_list_get(&clients, i, (void**)&cfd) == -1)
            continue;
        close(*cfd);
    }
    dl_list_destroy(&clients);

    /* Free connections list */
    dl_list_destroy(&connections);

    /* Free workers list */
    dl_list_destroy(&workers);
}

/*
 * Closes open file descriptors and frees hash maps
 */
static void
close_connections(void)
{
    bit_db_conn* conn;

    for (size_t i = 0; i < connections.num_elems; i++) {
        if (dl_list_get(&connections, i, (void**)&conn) == -1)
            continue;
        bit_db_destroy_conn(conn);
    }
}

/*
 * Joins running worker threads
 */
static void
stop_workers(void)
{
    int s;
    pthread_t* thread;

    /* Wake up all workers so they can check run variable */
    pthread_cond_broadcast(&clients_new);

    for (size_t i = 0; i < workers.num_elems; i++) {
        if (dl_list_get(&workers, i, (void**)&thread) == -1)
            continue;

        /* Causes any pending system calls to return EINTR */
        if ((s = pthread_kill(*thread, SIGUSR1)) != 0)
            syslog(LOG_ERR, "Failed to kill thread (%s)", strerror(s));
        if ((s = pthread_join(*thread, NULL)) != 0)
            syslog(LOG_ERR, "Failed to join thread (%s)", strerror(s));
    }
}

/*
 * Persists the hash tables to disk
 */
static void
persist_tables(void)
{
    bit_db_conn* conn;

    for (size_t i = 0; i < connections.num_elems; i++) {
        if (dl_list_get(&connections, i, (void**)&conn) == -1)
            continue;

        if (bit_db_persist_table(conn) == -1)
            printf("[INFO] Failed to persist connection\n");
        else
            printf("[INFO] Successfully persisted connection\n");
    }
}

static void
sig_int_handler(__attribute__((unused)) int signum)
{
    run = false;
}

static void
sig_usr_handler(__attribute__((unused)) int signum)
{
}

/*
 * Counts the number of log file segments in the database folder
 */
static size_t
count_num_segments(void)
{
    size_t count = 0;
    struct dirent* dir;
    DIR* dirp = opendir(DIRECTORY);
    regex_t regex;

    /* bit_db001 but not bit_db001.tb */
    if (regcomp(&regex, "^bit_db[0-9]+$", REG_EXTENDED) != 0)
        return 0;

    while ((dir = readdir(dirp)) != NULL)
        if (regexec(&regex, dir->d_name, 0, NULL, 0) == 0)
            count++;

    regfree(&regex);
    closedir(dirp);
    return (count > 0) ? count : 1;
}

/*
 * Counts the number of digits in a number
 */
static size_t
count_digits(size_t num)
{
    return snprintf(NULL, 0, "%lu", (unsigned long)num) - (num < 0);
}
