/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#pragma once

#include "queue.h"

/* Get the maximum/minimum of x and y */
#undef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#undef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))

#define STARTS_WITH(string, len, needle) ((len >= strlen(needle)) && strncasecmp(string, needle, strlen(needle)) == 0)

/* Securely free p */
#define FREE(p)          \
    do {                 \
        if (p != NULL) { \
            free(p);     \
            p = NULL;    \
        }                \
    } while (0)

/* Securely free single-linked list */
#define FREE_SLIST(l, type)              \
    do {                                 \
        type *walk = SLIST_FIRST(l);     \
        while (!SLIST_EMPTY(l)) {        \
            SLIST_REMOVE_HEAD(l, slist); \
            FREE(walk);                  \
            walk = SLIST_FIRST(l);       \
        }                                \
    } while (0)

/* Securely free tail queue */
#define FREE_TAILQ(l, type)                         \
    do {                                            \
        type *walk = TAILQ_FIRST(l);                \
        while (!TAILQ_EMPTY(l)) {                   \
            TAILQ_REMOVE(l, TAILQ_FIRST(l), tailq); \
            FREE(walk);                             \
            walk = TAILQ_FIRST(l);                  \
        }                                           \
    } while (0)

#if defined(DLOG)
#undef DLOG
#endif
/* Use cool logging macros */
#define DLOG(fmt, ...)                                                 \
    do {                                                               \
        if (config.verbose) {                                          \
            printf("[%s:%d] " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
        }                                                              \
    } while (0)

/* We will include libi3.h which define its own version of ELOG.
 * We want *our* version, so we undef the libi3 one. */
#if defined(ELOG)
#undef ELOG
#endif
#define ELOG(fmt, ...)                                                             \
    do {                                                                           \
        fprintf(stderr, "[%s:%d] ERROR: " fmt, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)
