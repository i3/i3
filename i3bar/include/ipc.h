/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2012 Axel Wagner and contributors (see also: LICENSE)
 *
 * ipc.c: Communicating with i3
 *
 */
#pragma once

#include <stdint.h>

/*
 * Initiate a connection to i3.
 * socket_path must be a valid path to the ipc_socket of i3
 *
 */
int init_connection(const char *socket_path);

/*
 * Destroy the connection to i3.
 *
 */
void destroy_connection(void);

/*
 * Sends a message to i3.
 * type must be a valid I3_IPC_MESSAGE_TYPE (see i3/ipc.h for further information)
 *
 */
int i3_send_msg(uint32_t type, const char *payload);

/*
 * Subscribe to all the i3-events, we need
 *
 */
void subscribe_events(void);
