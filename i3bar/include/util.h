/*
 * i3bar - an xcb-based status- and ws-bar for i3
 *
 * © 2010 Axel Wagner and contributors
 *
 * See file LICNSE for license information
 *
 */
#ifndef UTIL_H_
#define UTIL_H_

#include "queue.h"

/* Get the maximum/minimum of x and y */
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

/* Securely free p */
#define FREE(p) do { \
    if (p != NULL) { \
        free(p); \
        p = NULL; \
    } \
} while (0)

/* Securely fee single-linked list */
#define FREE_SLIST(l, type) do { \
    type *walk = SLIST_FIRST(l); \
    while (!SLIST_EMPTY(l)) { \
        SLIST_REMOVE_HEAD(l, slist); \
        FREE(walk); \
        walk = SLIST_FIRST(l); \
    } \
} while (0)

#endif

/* Securely fee tail-queues */
#define FREE_TAILQ(l, type) do { \
    type *walk = TAILQ_FIRST(l); \
    while (!TAILQ_EMPTY(l)) { \
        TAILQ_REMOVE(l, TAILQ_FIRST(l), tailq); \
        FREE(walk); \
        walk = TAILQ_FIRST(l); \
    } \
} while (0)

/* Use cool logging-macros */
#define DLOG(fmt, ...) do { \
    if (config.verbose) { \
        printf("[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
    } \
} while(0)

#define ELOG(fmt, ...) do { \
    fprintf(stderr, "[%s:%d] ERROR: " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
} while(0)
