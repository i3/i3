/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * ipc.c: UNIX domain socket IPC (initialization, client handling, protocol).
 *
 */
#pragma once

#include <config.h>

#include <ev.h>
#include <stdbool.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_parse.h>

#include "data.h"
#include "tree.h"
#include "configuration.h"

#include "i3/ipc.h"

extern char *current_socketpath;

typedef struct ipc_client {
    int fd;

    /* The events which this client wants to receive */
    int num_events;
    char **events;

    /* For clients which subscribe to the tick event: whether the first tick
     * event has been sent by i3. */
    bool first_tick_sent;

    TAILQ_ENTRY(ipc_client)
    clients;
} ipc_client;

/*
 * Callback type for the different message types.
 *
 * message is the raw packet, as received from the UNIX domain socket. size
 * is the remaining size of bytes for this packet.
 *
 * message_size is the size of the message as the sender specified it.
 * message_type is the type of the message as the sender specified it.
 *
 */
typedef void (*handler_t)(int, uint8_t *, int, uint32_t, uint32_t);

/* Macro to declare a callback */
#define IPC_HANDLER(name)                                      \
    static void handle_##name(int fd, uint8_t *message,        \
                              int size, uint32_t message_size, \
                              uint32_t message_type)

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

/**
 * Sends the specified event to all IPC clients which are currently connected
 * and subscribed to this kind of event.
 *
 */
void ipc_send_event(const char *event, uint32_t message_type, const char *payload);

/**
 * Calls to ipc_shutdown() should provide a reason for the shutdown.
 */
typedef enum {
    SHUTDOWN_REASON_RESTART,
    SHUTDOWN_REASON_EXIT
} shutdown_reason_t;

/**
 * Calls shutdown() on each socket and closes it.
 *
 */
void ipc_shutdown(shutdown_reason_t reason);

void dump_node(yajl_gen gen, Con *con, bool inplace_restart);

/**
 * Generates a json workspace event. Returns a dynamically allocated yajl
 * generator. Free with yajl_gen_free().
 */
yajl_gen ipc_marshal_workspace_event(const char *change, Con *current, Con *old);

/**
 * For the workspace events we send, along with the usual "change" field, also
 * the workspace container in "current". For focus events, we send the
 * previously focused workspace in "old".
 */
void ipc_send_workspace_event(const char *change, Con *current, Con *old);

/**
 * For the window events we send, along the usual "change" field,
 * also the window container, in "container".
 */
void ipc_send_window_event(const char *property, Con *con);

/**
 * For the barconfig update events, we send the serialized barconfig.
 */
void ipc_send_barconfig_update_event(Barconfig *barconfig);

/**
 * For the binding events, we send the serialized binding struct.
 */
void ipc_send_binding_event(const char *event_type, Binding *bind);
