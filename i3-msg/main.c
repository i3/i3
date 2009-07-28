/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * i3-msg/main.c: Utility which sends messages to a running i3-instance using
 * IPC via UNIX domain sockets.
 *
 * This serves as an example for how to send your own messages to i3.
 * Additionally, it’s even useful sometimes :-).
 *
 */
#include <ev.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <stdint.h>
#include <getopt.h>

static void ipc_send_message(int sockfd, uint32_t message_size,
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

int main(int argc, char *argv[]) {
        char *socket_path = "/tmp/i3-ipc.sock";
        int o, option_index = 0;

        static struct option long_options[] = {
                {"socket", required_argument, 0, 's'},
                {"type", required_argument, 0, 't'},
                {"version", no_argument, 0, 'v'},
                {"help", no_argument, 0, 'h'},
                {0, 0, 0, 0}
        };

        char *options_string = "s:t:vh";

        while ((o = getopt_long(argc, argv, options_string, long_options, &option_index)) != -1) {
                if (o == 's') {
                        socket_path = strdup(optarg);
                        break;
                } else if (o == 't') {
                        printf("currently only commands are implemented\n");
                } else if (o == 'v') {
                        printf("i3-msg " I3_VERSION);
                        return 0;
                } else if (o == 'h') {
                        printf("i3-msg " I3_VERSION);
                        printf("i3-msg [-s <socket>] [-t <type>] <message>\n");
                        return 0;
                }
        }

        if (optind >= argc) {
                fprintf(stderr, "Error: missing message\n");
                fprintf(stderr, "i3-msg [-s <socket>] [-t <type>] <message>\n");
                return 1;
        }

        int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_LOCAL;
        strcpy(addr.sun_path, socket_path);
        if (connect(sockfd, &addr, sizeof(struct sockaddr_un)) < 0)
                err(-1, "Could not connect to i3");

        ipc_send_message(sockfd, strlen(argv[optind]), 0, (uint8_t*)argv[optind]);

        close(sockfd);

        return 0;
}
