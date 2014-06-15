/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2013 Michael Stapelberg and contributors (see also: LICENSE)
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

    size_t sent_bytes = 0;
    int n = 0;

    /* This first loop is basically unnecessary. No operating system has
     * buffers which cannot fit 14 bytes into them, so the write() will only be
     * called once. */
    while (sent_bytes < sizeof(i3_ipc_header_t)) {
        if ((n = write(sockfd, ((void *)&header) + sent_bytes, sizeof(i3_ipc_header_t) - sent_bytes)) == -1) {
            if (errno == EAGAIN)
                continue;
            return -1;
        }

        sent_bytes += n;
    }

    sent_bytes = 0;

    while (sent_bytes < message_size) {
        if ((n = write(sockfd, payload + sent_bytes, message_size - sent_bytes)) == -1) {
            if (errno == EAGAIN)
                continue;
            return -1;
        }

        sent_bytes += n;
    }

    return 0;
}
