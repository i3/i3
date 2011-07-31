/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <err.h>

/*
 * Formats a message (payload) of the given size and type and sends it to i3 via
 * the given socket file descriptor.
 *
 */
void ipc_send_message(int sockfd, uint32_t message_size,
                      uint32_t message_type, uint8_t *payload) {
        int buffer_size = strlen("i3-ipc") + sizeof(uint32_t) + sizeof(uint32_t) + message_size;
        char msg[buffer_size];
        char *walk = msg;

        strcpy(walk, "i3-ipc");
        walk += strlen("i3-ipc");
        memcpy(walk, &message_size, sizeof(uint32_t));
        walk += sizeof(uint32_t);
        memcpy(walk, &message_type, sizeof(uint32_t));
        walk += sizeof(uint32_t);
        memcpy(walk, payload, message_size);

        int sent_bytes = 0;
        int bytes_to_go = buffer_size;
        while (sent_bytes < bytes_to_go) {
                int n = write(sockfd, msg + sent_bytes, bytes_to_go);
                if (n == -1)
                        err(EXIT_FAILURE, "write() failed");

                sent_bytes += n;
                bytes_to_go -= n;
        }
}

/*
 * Connects to the i3 IPC socket and returns the file descriptor for the
 * socket. die()s if anything goes wrong.
 *
 */
int connect_ipc(char *socket_path) {
	int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
        if (sockfd == -1)
                err(EXIT_FAILURE, "Could not create socket");

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_LOCAL;
        strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
        if (connect(sockfd, (const struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0)
                err(EXIT_FAILURE, "Could not connect to i3");

	return sockfd;
}
