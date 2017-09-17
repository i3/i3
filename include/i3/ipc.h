/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * This public header defines the different constants and message types to use
 * for the IPC interface to i3 (see docs/ipc for more information).
 *
 */
#pragma once

#include <stdint.h>

typedef struct i3_ipc_header {
    /* 6 = strlen(I3_IPC_MAGIC) */
    char magic[6];
    uint32_t size;
    uint32_t type;
} __attribute__((packed)) i3_ipc_header_t;

/*
 * Messages from clients to i3
 *
 */

/** Never change this, only on major IPC breakage (don’t do that) */
#define I3_IPC_MAGIC "i3-ipc"

/** Deprecated: use I3_IPC_MESSAGE_TYPE_RUN_COMMAND */
#define I3_IPC_MESSAGE_TYPE_COMMAND 0

/** The payload of the message will be interpreted as a command */
#define I3_IPC_MESSAGE_TYPE_RUN_COMMAND 0

/** Requests the current workspaces from i3 */
#define I3_IPC_MESSAGE_TYPE_GET_WORKSPACES 1

/** Subscribe to the specified events */
#define I3_IPC_MESSAGE_TYPE_SUBSCRIBE 2

/** Requests the current outputs from i3 */
#define I3_IPC_MESSAGE_TYPE_GET_OUTPUTS 3

/** Requests the tree layout from i3 */
#define I3_IPC_MESSAGE_TYPE_GET_TREE 4

/** Request the current defined marks from i3 */
#define I3_IPC_MESSAGE_TYPE_GET_MARKS 5

/** Request the configuration for a specific 'bar' */
#define I3_IPC_MESSAGE_TYPE_GET_BAR_CONFIG 6

/** Request the i3 version */
#define I3_IPC_MESSAGE_TYPE_GET_VERSION 7

/** Request a list of configured binding modes. */
#define I3_IPC_MESSAGE_TYPE_GET_BINDING_MODES 8

/** Request the raw last loaded i3 config. */
#define I3_IPC_MESSAGE_TYPE_GET_CONFIG 9

/*
 * Messages from i3 to clients
 *
 */
#define I3_IPC_REPLY_TYPE_COMMAND 0
#define I3_IPC_REPLY_TYPE_WORKSPACES 1
#define I3_IPC_REPLY_TYPE_SUBSCRIBE 2
#define I3_IPC_REPLY_TYPE_OUTPUTS 3
#define I3_IPC_REPLY_TYPE_TREE 4
#define I3_IPC_REPLY_TYPE_MARKS 5
#define I3_IPC_REPLY_TYPE_BAR_CONFIG 6
#define I3_IPC_REPLY_TYPE_VERSION 7
#define I3_IPC_REPLY_TYPE_BINDING_MODES 8
#define I3_IPC_REPLY_TYPE_CONFIG 9

/*
 * Events from i3 to clients. Events have the first bit set high.
 *
 */
#define I3_IPC_EVENT_MASK (1 << 31)

/* The workspace event will be triggered upon changes in the workspace list */
#define I3_IPC_EVENT_WORKSPACE (I3_IPC_EVENT_MASK | 0)

/* The output event will be triggered upon changes in the output list */
#define I3_IPC_EVENT_OUTPUT (I3_IPC_EVENT_MASK | 1)

/* The output event will be triggered upon mode changes */
#define I3_IPC_EVENT_MODE (I3_IPC_EVENT_MASK | 2)

/* The window event will be triggered upon window changes */
#define I3_IPC_EVENT_WINDOW (I3_IPC_EVENT_MASK | 3)

/** Bar config update will be triggered to update the bar config */
#define I3_IPC_EVENT_BARCONFIG_UPDATE (I3_IPC_EVENT_MASK | 4)

/** The binding event will be triggered when bindings run */
#define I3_IPC_EVENT_BINDING (I3_IPC_EVENT_MASK | 5)

/** The shutdown event will be triggered when the ipc shuts down */
#define I3_IPC_EVENT_SHUTDOWN (I3_IPC_EVENT_MASK | 6)
