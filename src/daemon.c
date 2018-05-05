#include "bit_db.h"
#include "dl_list.h"
#include "error_functions.h"
#include "helper_functions.h"
#include "inet_sockets.h"
#include "sl_list.h"
#include <errno.h>
#include <pthread.h>
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

/******************** RESPONSES ************************/

#define OK "+OK"
#define BENOKEY "-NOKEY Get request doesn't include a key\r\n"

/******************************************************/

bool volatile run = true;

static dl_list connections;
static pthread_mutex_t conns_mtx;

static dl_list clients;
static pthread_cond_t clients_new = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t clients_mtx;

static dl_list workers;

/******************************************************/

/******************************************************/

static void *handle_request(void *cfd);
static ssize_t send_response(int cfd, char *msg, size_t bytes);
static int dequeue_client(int **cfd);
static int enqueue_client(int cfd);

/*
 * Protocol functions
 */
static ssize_t handle_get(int cfd, char *line);

static void init_data(void);
static void init_mutex(void);
static void open_connections(void);
static void start_workers(void);
static void handle_signals(void);

static void destroy_data(void);
static void close_connections(void);
static void stop_workers(void);
static void persist_tables(void);

static void sig_int_handler(int signum);
static void sig_usr_handler(int signum);

/******************************************************/

int
main(void)
{
    int lfd, cfd;

    init_data();
    init_mutex();
    open_connections();
    start_workers();
    handle_signals();

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

        if (enqueue_client(cfd) == -1) {
            errMsg("enqueue_client() %d", cfd);
            break;
        }

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
    pthread_mutex_init(&clients_mtx, &attr)) != 0)                if
    (bit_db_connect(connection, pathname) == -1) {
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
static void *
handle_request(__attribute__((unused)) void *fd)
{
    int s;
    ssize_t written;
    int *cfd = NULL;
    ssize_t num_read = 0;
    char line[BUF_SIZE];
    char *line_dup;
    char *token;

WAIT:
    s = pthread_mutex_lock(&clients_mtx);
    if (s != 0)
        errExitEN(s, "pthread_mutex_lock()");

    s = pthread_cond_wait(&clients_new, &clients_mtx);
    if (s != 0)
        errExitEN(s, "pthread_cond_wait()");

    /* clients_mtx is locked when pthread_cond_wait returns */
    if (!run) {
        pthread_mutex_unlock(&clients_mtx);
        return NULL;
    }

    /* Should always be at least 1 client because clients_new was triggered */
    if (dequeue_client(&cfd) == -1)
        errExit("dequeue_client()");

    while (true) {
        /* Serve the client */
        while (run && ((num_read = read_line(*cfd, line, BUF_SIZE)) > 0)) {
            line_dup = line;
            token = strsep(&line_dup, " ");
            if (token == NULL)
                continue;

            written = 0;
            strlwr(token);
            if (strncmp("get", token, 3) == 0) {
                written = handle_get(*cfd, line_dup);
            }
            else
                printf("Unrecognised token!\n");

            /* An error occured in handling request, could be a program
             * ending interrupt or some other error
             */
            if (written < 0) {
                close(*cfd);
                free(cfd);
                return NULL;
            }
        }
        close(*cfd);
        free(cfd);

        /* If read failed due to interrupt check for exit condition */
        if (num_read == -1 && errno == EINTR)
            if (!run)
                break;

        if (dequeue_client(&cfd) == 0) {
            /* Another client to serve */
            continue;
        }
        else if (run) {
            /* No clients left to serve and exit condition false */
            goto WAIT;
        }

        /* exit condition */
        break;
    }
    return NULL;
}

/*
 * Writes `msg` to the client socket. `bytes` must be
 * strlen(msg) + 1 or sizeof(const msg). Retries if
 * an interrupt occurs that is unrelated to program exit.
 *
 * Returns -1 on program exiting interrupt or other error.
 */
static ssize_t
send_response(int cfd, char *msg, size_t bytes)
{
    ssize_t num_written;
    size_t tot_written = 0;

RETRY:
    if ((num_written = write(cfd, msg, bytes - tot_written)) != -1) {
        /* zero or more bytes written */
        tot_written += num_written;
        if (tot_written < bytes)
            goto RETRY;
    }
    else {
        /* An error occurred */
        if (errno == EINTR && run)
            goto RETRY;
        else
            return -1;
    }
    return tot_written;
}

/*
 * Dequeues a client and points cfd to the descriptor
 *
 * Post-condition: clients_mtx will be unlocked
 */
static int
dequeue_client(int **cfd)
{
    int s, status;

    /* errno will be EDEADLK if the mutex is already owned */
    if ((s = pthread_mutex_lock(&clients_mtx)) != 0 && s != EDEADLK)
        errExitEN(s, "pthread_mutex_lock()");

    status = dl_list_dequeue(&clients, (void **)cfd);

    if ((s = pthread_mutex_unlock(&clients_mtx)) != 0)
        errExitEN(s, "pthread_mutex_unlock()");

    return status;
}

/*
 * Queues a client
 *
 * Post-conditions: clients_mtx will be unlocked,
 *                  if a worker is pending it will be woken
 */
static int
enqueue_client(int cfd)
{
    int s, status;

    /* errno will be EDEADLK if the mutex is already owned */
    if ((s = pthread_mutex_lock(&clients_mtx)) != 0 && s != EDEADLK)
        errExitEN(s, "pthread_mutex_lock()");

    status = dl_list_enqueue(&clients, &cfd);

    if ((s = pthread_mutex_unlock(&clients_mtx)) != 0)
        errExitEN(s, "pthread_mutex_unlock()");

    /* Wakes a single pending worker if exists */
    if ((s = pthread_cond_signal(&clients_new)) != 0)
        errExitEN(s, "pthread_cond_signal()");

    return status;
}

/*
 * Handles a get request, e.g. "GET key CRLF"
 */
static ssize_t
handle_get(int cfd, char *line)
{
    if (line == NULL || *line == ' ')
        return send_response(cfd, BENOKEY, sizeof(BENOKEY));

    if (send_response(cfd, OK, sizeof(OK)) == -1)
        return -1;

    printf("%d\n", (int)line);
    /* The number of bytes in the value */
    return send_response(cfd, " 32\r\n", 6);
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
 * Initialises the mutexes
 */
static void
init_mutex(void)
{
    int s;
    pthread_mutexattr_t attr;

    if ((s = pthread_mutexattr_init(&attr)) != 0)
        errExitEN(s, "pthread_mutexattr_init()");

    /* Defaults are fine for conns_mtx */
    if ((s = pthread_mutex_init(&conns_mtx, &attr)) != 0)
        errExitEN(s, "pthread_mutex_init() conns_mtx");

    /* clients_mtx should be error checking */
    if ((s = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK)) != 0)
        errExitEN(s, "pthread_mutexattr_settype()");
    if ((s = pthread_mutex_init(&clients_mtx, &attr)) != 0)
        errExitEN(s, "pthread_mutex_init() clients_mtx");
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
        if (dl_list_stack_peek(&workers, (void **)&stored_thread) == -1)
            errExit("dl_list_stack_peek()");

        s = pthread_create(stored_thread, NULL, handle_request, NULL);
        if (s != 0)
            errExitEN(s, "pthread_create");
    }
}

/*
 * Register signal handlers
 */
static void
handle_signals(void)
{
    struct sigaction sa;

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
}

/*
 * Deallocates memory from global data structures
 */
static void
destroy_data(void)
{
    int *cfd;

    /* Close connection to any remaining clients */
    for (size_t i = 0; i < clients.num_elems; i++) {
        if (dl_list_get(&clients, i, (void **)&cfd) == -1)
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
    bit_db_conn *conn;

    for (size_t i = 0; i < connections.num_elems; i++) {
        if (dl_list_get(&connections, i, (void **)&conn) == -1)
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
    pthread_t *thread;

    /* Wake up all workers so they can check run variable */
    pthread_cond_broadcast(&clients_new);

    for (size_t i = 0; i < workers.num_elems; i++) {
        if (dl_list_get(&workers, i, (void **)&thread) == -1)
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
    bit_db_conn *conn;

    for (size_t i = 0; i < connections.num_elems; i++) {
        if (dl_list_get(&connections, i, (void **)&conn) == -1)
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
