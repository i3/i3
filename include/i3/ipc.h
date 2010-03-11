/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * This public header defines the different constants and message types to use
 * for the IPC interface to i3 (see docs/ipc for more information).
 *
 */

#ifndef _I3_IPC_H
#define _I3_IPC_H

/*
 * Messages from clients to i3
 *
 */

/** Never change this, only on major IPC breakage (don’t do that) */
#define I3_IPC_MAGIC 			"i3-ipc"

/** The payload of the message will be interpreted as a command */
#define I3_IPC_MESSAGE_TYPE_COMMAND	        0

/** Requests the current workspaces from i3 */
#define I3_IPC_MESSAGE_TYPE_GET_WORKSPACES      1

/*
 * Messages from i3 to clients
 *
 */

/** Workspaces reply type */
#define I3_IPC_REPLY_TYPE_WORKSPACES            1

#endif
