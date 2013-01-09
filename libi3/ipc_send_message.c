/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
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
int ipc_send_message(int sockfd, uint32_t message_size,
                     uint32_t message_type, const uint8_t *payload) {
    int buffer_size = strlen(I3_IPC_MAGIC) + sizeof(uint32_t) + sizeof(uint32_t) + message_size;
    char msg[buffer_size];
    char *walk = msg;

    strncpy(walk, I3_IPC_MAGIC, buffer_size - 1);
    walk += strlen(I3_IPC_MAGIC);
    memcpy(walk, &message_size, sizeof(uint32_t));
    walk += sizeof(uint32_t);
    memcpy(walk, &message_type, sizeof(uint32_t));
    walk += sizeof(uint32_t);
    memcpy(walk, payload, message_size);

    int sent_bytes = 0;
    while (sent_bytes < buffer_size) {
        int n = write(sockfd, msg + sent_bytes, buffer_size - sent_bytes);
        if (n == -1) {
            if (errno == EAGAIN)
                continue;
            return -1;
        }

        sent_bytes += n;
    }

    return 0;
}
