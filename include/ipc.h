/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */

#ifndef _IPC_H
#define _IPC_H

#include <ev.h>

#include "i3/ipc.h"

/**
 * Handler for activity on the listening socket, meaning that a new client
 * has just connected and we should accept() him. Sets up the event handler
 * for activity on the new connection and inserts the file descriptor into
 * the list of clients.
 *
 */
void ipc_new_client(EV_P_ struct ev_io *w, int revents);

/**
 * Creates the UNIX domain socket at the given path, sets it to non-blocking
 * mode, bind()s and listen()s on it.
 *
 */
int ipc_create_socket(const char *filename);

#endif
