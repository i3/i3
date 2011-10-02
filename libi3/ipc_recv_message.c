/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2011 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include <i3/ipc.h>

#include "libi3.h"

/*
 * Reads a message from the given socket file descriptor and stores its length
 * (reply_length) as well as a pointer to its contents (reply).
 *
 * Returns -1 when read() fails, errno will remain.
 * Returns -2 when the IPC protocol is violated (invalid magic, unexpected
 * message type, EOF instead of a message). Additionally, the error will be
 * printed to stderr.
 * Returns 0 on success.
 *
 */
int ipc_recv_message(int sockfd, uint32_t message_type,
                     uint32_t *reply_length, uint8_t **reply) {
    /* Read the message header first */
    uint32_t to_read = strlen(I3_IPC_MAGIC) + sizeof(uint32_t) + sizeof(uint32_t);
    char msg[to_read];
    char *walk = msg;

    uint32_t read_bytes = 0;
    while (read_bytes < to_read) {
        int n = read(sockfd, msg + read_bytes, to_read);
        if (n == -1)
            return -1;
        if (n == 0) {
            fprintf(stderr, "IPC: received EOF instead of reply\n");
            return -2;
        }

        read_bytes += n;
        to_read -= n;
    }

    if (memcmp(walk, I3_IPC_MAGIC, strlen(I3_IPC_MAGIC)) != 0) {
        fprintf(stderr, "IPC: invalid magic in reply\n");
        return -2;
    }

    walk += strlen(I3_IPC_MAGIC);
    *reply_length = *((uint32_t*)walk);
    walk += sizeof(uint32_t);
    if (*((uint32_t*)walk) != message_type) {
        fprintf(stderr, "IPC: unexpected reply type (got %d, expected %d)\n", *((uint32_t*)walk), message_type);
        return -2;
    }
    walk += sizeof(uint32_t);

    *reply = smalloc(*reply_length);

    to_read = *reply_length;
    read_bytes = 0;
    while (read_bytes < to_read) {
        int n = read(sockfd, *reply + read_bytes, to_read);
        if (n == -1)
            return -1;

        read_bytes += n;
        to_read -= n;
    }

    return 0;
}
