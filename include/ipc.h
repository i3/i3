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

void ipc_new_client(EV_P_ struct ev_io *w, int revents);

int ipc_create_socket(const char *filename);

#endif
