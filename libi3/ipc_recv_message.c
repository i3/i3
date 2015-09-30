/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include <i3/ipc.h>

#include "libi3.h"

/*
 * Reads a message from the given socket file descriptor and stores its length
 * (reply_length) as well as a pointer to its contents (reply).
 *
 * Returns -1 when read() fails, errno will remain.
 * Returns -2 on EOF.
 * Returns -3 when the IPC protocol is violated (invalid magic, unexpected
 * message type, EOF instead of a message). Additionally, the error will be
 * printed to stderr.
 * Returns 0 on success.
 *
 */
int ipc_recv_message(int sockfd, uint32_t *message_type,
                     uint32_t *reply_length, uint8_t **reply) {
    /* Read the message header first */
    const uint32_t to_read = strlen(I3_IPC_MAGIC) + sizeof(uint32_t) + sizeof(uint32_t);
    char msg[to_read];
    char *walk = msg;

    uint32_t read_bytes = 0;
    while (read_bytes < to_read) {
        int n = read(sockfd, msg + read_bytes, to_read - read_bytes);
        if (n == -1)
            return -1;
        if (n == 0) {
            return -2;
        }

        read_bytes += n;
    }

    if (memcmp(walk, I3_IPC_MAGIC, strlen(I3_IPC_MAGIC)) != 0) {
        ELOG("IPC: invalid magic in reply\n");
        return -3;
    }

    walk += strlen(I3_IPC_MAGIC);
    memcpy(reply_length, walk, sizeof(uint32_t));
    walk += sizeof(uint32_t);
    if (message_type != NULL)
        memcpy(message_type, walk, sizeof(uint32_t));

    *reply = smalloc(*reply_length);

    read_bytes = 0;
    int n;
    while (read_bytes < *reply_length) {
        if ((n = read(sockfd, *reply + read_bytes, *reply_length - read_bytes)) == -1) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return -1;
        }

        read_bytes += n;
    }

    return 0;
}
