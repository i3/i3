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

#endif
