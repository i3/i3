#ifndef UTIL_H_
#define UTIL_H_

/* Securely free p */
#define FREE(p) do { \
	if (p != NULL) { \
		free(p); \
		p = NULL; \
	} \
} while (0)

/* Securely fee single-linked list */
#define FREE_LIST(l, type) do { \
	type* FREE_LIST_TMP; \
	while (l != NULL) { \
		FREE_LIST_TMP = l; \
		free(l); \
		l = l->next; \
	} \
} while (0)

#endif
