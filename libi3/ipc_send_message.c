/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <err.h>
#include <errno.h>

#include <i3/ipc.h>

#include "libi3.h"

/*
 * Formats a message (payload) of the given size and type and sends it to i3 via
 * the given socket file descriptor.
 *
 * Returns -1 when write() fails, errno will remain.
 * Returns 0 on success.
 *
 */
int ipc_send_message(int sockfd, const uint32_t message_size,
                     const uint32_t message_type, const uint8_t *payload) {
    const i3_ipc_header_t header = {
        /* We don’t use I3_IPC_MAGIC because it’s a 0-terminated C string. */
        .magic = {'i', '3', '-', 'i', 'p', 'c'},
        .size = message_size,
        .type = message_type};

    if (writeall(sockfd, ((void *)&header), sizeof(i3_ipc_header_t)) == -1)
        return -1;

    if (writeall(sockfd, payload, message_size) == -1)
        return -1;

    return 0;
}
