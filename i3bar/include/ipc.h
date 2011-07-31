/*
 * i3bar - an xcb-based status- and ws-bar for i3
 *
 * © 2010-2011 Axel Wagner and contributors
 *
 * See file LICNSE for license information
 *
 */
#ifndef IPC_H_
#define IPC_H_

#include <stdint.h>

/*
 * Initiate a connection to i3.
 * socket-path must be a valid path to the ipc_socket of i3
 *
 */
int init_connection(const char *socket_path);

/*
 * Destroy the connection to i3.
 *
 */
void destroy_connection();

/*
 * Sends a Message to i3.
 * type must be a valid I3_IPC_MESSAGE_TYPE (see i3/ipc.h for further information)
 *
 */
int i3_send_msg(uint32_t type, const char* payload);

/*
 * Subscribe to all the i3-events, we need
 *
 */
void subscribe_events();

#endif
