/**********************************************************************\
*                Copyright (C) Michael Kerrisk, 2010.                  *
*                                                                      *
* This program is free software. You may use, modify, and redistribute *
* it under the terms of the GNU Affero General Public License as       *
* published by the Free Software Foundation, either version 3 or (at   *
* your option) any later version. This program is distributed without  *
* any warranty. See the file COPYING for details.                      *
\**********************************************************************/

/* inet_sockets.c
   A package of useful routines for Internet domain sockets.
*/
#define _DEFAULT_SOURCE   /* To get NI_MAXHOST and NI_MAXSERV                  \
                         definitions from <netdb.h> */
#include "inet_sockets.h" /* Declares functions defined here */
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* The following arguments are common to several of the routines
   below:
        'host':         NULL for loopback IP address, or
                        a host name or numeric IP address
        'service':      either a name or a port number
        'type':         either SOCK_STREAM or SOCK_DGRAM
*/

/* Create socket and connect it to the address specified by
  'host' + 'service'/'type'. Return socket descriptor on success,
  or -1 on error */

extern bool volatile run;

static int
inetPassiveSocket(const char* service,
                  int type,
                  socklen_t* addrlen,
                  bool doListen,
                  int backlog);

int
inetConnect(const char* host, const char* service, int type)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_family = AF_UNSPEC; /* Allows IPv4 or IPv6 */
    hints.ai_socktype = type;

    s = getaddrinfo(host, service, &hints, &result);
    if (s != 0) {
        errno = ENOSYS;
        return -1;
    }

    /* Walk through returned list until we find an address structure
       that can be used to successfully connect a socket */

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue; /* On error, try next address */

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break; /* Success */

        /* Connect failed: close this socket and try next address */

        close(sfd);
    }

    freeaddrinfo(result);

    return (rp == NULL) ? -1 : sfd;
}

/* Create an Internet domain socket and bind it to the address
   { wildcard-IP-address + 'service'/'type' }.
   If 'doListen' is true, then make this a listening socket (by
   calling listen() with 'backlog'), with the SO_REUSEADDR option set.
   If 'addrLen' is not NULL, then use it to return the size of the
   address structure for the address family for this socket.
   Return the socket descriptor on success, or -1 on error. */

static int /* Public interfaces: inetBind() and inetListen() */
inetPassiveSocket(const char* service,
                  int type,
                  socklen_t* addrlen,
                  bool doListen,
                  int backlog)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, optval, s;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_socktype = type;
    hints.ai_family = AF_UNSPEC; /* Allows IPv4 or IPv6 */
    hints.ai_flags = AI_PASSIVE; /* Use wildcard IP address */

    s = getaddrinfo(NULL, service, &hints, &result);
    if (s != 0)
        return -1;

    /* Walk through returned list until we find an address structure
       that can be used to successfully create and bind a socket */

    optval = 1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue; /* On error, try next address */

        if (doListen) {
            if (setsockopt(
                  sfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) ==
                -1) {
                close(sfd);
                freeaddrinfo(result);
                return -1;
            }
        }

        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break; /* Success */

        /* bind() failed: close this socket and try next address */

        close(sfd);
    }

    if (rp != NULL && doListen) {
        if (listen(sfd, backlog) == -1) {
            freeaddrinfo(result);
            return -1;
        }
    }

    if (rp != NULL && addrlen != NULL)
        *addrlen = rp->ai_addrlen; /* Return address structure size */

    freeaddrinfo(result);

    return (rp == NULL) ? -1 : sfd;
}

/* Create stream socket, bound to wildcard IP address + port given in
  'service'. Make the socket a listening socket, with the specified
  'backlog'. Return socket descriptor on success, or -1 on error. */

int
inetListen(const char* service, int backlog, socklen_t* addrlen)
{
    return inetPassiveSocket(service, SOCK_STREAM, addrlen, true, backlog);
}

/* Create socket bound to wildcard IP address + port given in
   'service'. Return socket descriptor on success, or -1 on error. */

int
inetBind(const char* service, int type, socklen_t* addrlen)
{
    return inetPassiveSocket(service, type, addrlen, false, 0);
}

/* Given a socket address in 'addr', whose length is specified in
   'addrlen', return a null-terminated string containing the host and
   service names in the form "(hostname, port#)". The string is
   returned in the buffer pointed to by 'addrStr', and this value is
   also returned as the function result. The caller must specify the
   size of the 'addrStr' buffer in 'addrStrLen'. */

char*
inetAddressStr(const struct sockaddr* addr,
               socklen_t addrlen,
               char* addrStr,
               int addrStrLen)
{
    char host[NI_MAXHOST], service[NI_MAXSERV];

    if (getnameinfo(addr,
                    addrlen,
                    host,
                    NI_MAXHOST,
                    service,
                    NI_MAXSERV,
                    NI_NUMERICSERV) == 0)
        snprintf(addrStr, addrStrLen, "(%s, %s)", host, service);
    else
        snprintf(addrStr, addrStrLen, "(?UNKNOWN?)");

    return addrStr;
}

ssize_t
read_line(int sfd, void* buffer, size_t n)
{
    ssize_t num_read;
    size_t tot_read;
    char* buf;
    char ch;

    if (n <= 0 || buffer == NULL) {
        errno = EINVAL;
        return -1;
    }

    buf = buffer;

    tot_read = 0;
    while (run) {
        num_read = read(sfd, &ch, 1);

        if (num_read == -1) {
            if (errno == EINTR)
                if (run)
                    continue;
                else
                    return -1;
            else
                return -1;
        }
        else if (num_read == 0) {
            if (tot_read == 0)
                return 0;
            else
                break;
        }
        else {
            if (tot_read < n - 1) {
                tot_read++;
                *buf++ = ch;
            }
        }

        if (ch == '\n') {
            *(buf - 1) = '\0';
            break;
        }
    }
    return tot_read;
}
