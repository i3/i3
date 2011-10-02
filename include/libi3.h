/*
 * vim:ts=4:sw=4:expandtab
 */

#ifndef _LIBI3_H
#define _LIBI3_H

/**
 * Try to get the socket path from X11 and return NULL if it doesn’t work.
 *
 * The memory for the socket path is dynamically allocated and has to be
 * free()d by the caller.
 *
 */
char *socket_path_from_x11();

/**
 * Safe-wrapper around malloc which exits if malloc returns NULL (meaning that
 * there is no more memory available)
 *
 */
void *smalloc(size_t size);

/**
 * Safe-wrapper around calloc which exits if malloc returns NULL (meaning that
 * there is no more memory available)
 *
 */
void *scalloc(size_t size);

/**
 * Safe-wrapper around realloc which exits if realloc returns NULL (meaning
 * that there is no more memory available).
 *
 */
void *srealloc(void *ptr, size_t size);

/**
 * Safe-wrapper around strdup which exits if malloc returns NULL (meaning that
 * there is no more memory available)
 *
 */
char *sstrdup(const char *str);

#endif
