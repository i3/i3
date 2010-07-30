#ifndef UTIL_H_
#define UTIL_H_

#include "queue.h"

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
