/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
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
#include <stdbool.h>
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

#include <i3/ipc.h>

/*
 * Formats a message (payload) of the given size and type and sends it to i3 via
 * the given socket file descriptor.
 *
 */
static void ipc_send_message(int sockfd, uint32_t message_size,
                             uint32_t message_type, uint8_t *payload) {
        int buffer_size = strlen(I3_IPC_MAGIC) + sizeof(uint32_t) + sizeof(uint32_t) + message_size;
        char msg[buffer_size];
        char *walk = msg;

        strcpy(walk, I3_IPC_MAGIC);
        walk += strlen(I3_IPC_MAGIC);
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

static void ipc_recv_message(int sockfd, uint32_t message_type,
                             uint32_t *reply_length, uint8_t **reply) {
        /* Read the message header first */
        uint32_t to_read = strlen(I3_IPC_MAGIC) + sizeof(uint32_t) + sizeof(uint32_t);
        char msg[to_read];
        char *walk = msg;

        uint32_t read_bytes = 0;
        while (read_bytes < to_read) {
                int n = read(sockfd, msg + read_bytes, to_read);
                if (n == -1)
                        err(EXIT_FAILURE, "read() failed");
                if (n == 0)
                        errx(EXIT_FAILURE, "received EOF instead of reply");

                read_bytes += n;
                to_read -= n;
        }

        if (memcmp(walk, I3_IPC_MAGIC, strlen(I3_IPC_MAGIC)) != 0)
                errx(EXIT_FAILURE, "invalid magic in reply");

        walk += strlen(I3_IPC_MAGIC);
        *reply_length = *((uint32_t*)walk);
        walk += sizeof(uint32_t);
        if (*((uint32_t*)walk) != message_type)
                errx(EXIT_FAILURE, "unexpected reply type (got %d, expected %d)", *((uint32_t*)walk), message_type);
        walk += sizeof(uint32_t);

        *reply = malloc(*reply_length);
        if ((*reply) == NULL)
                err(EXIT_FAILURE, "malloc() failed");

        to_read = *reply_length;
        read_bytes = 0;
        while (read_bytes < to_read) {
                int n = read(sockfd, *reply + read_bytes, to_read);
                if (n == -1)
                        err(EXIT_FAILURE, "read() failed");

                read_bytes += n;
                to_read -= n;
        }
}

int main(int argc, char *argv[]) {
        char *socket_path;
        if ((socket_path = getenv("I3SOCK")) == NULL) {
            socket_path = "/tmp/i3-ipc.sock";
        }
        int o, option_index = 0;
        int message_type = I3_IPC_MESSAGE_TYPE_COMMAND;
        char *payload = "";
        bool quiet = false;

        static struct option long_options[] = {
                {"socket", required_argument, 0, 's'},
                {"type", required_argument, 0, 't'},
                {"version", no_argument, 0, 'v'},
                {"quiet", no_argument, 0, 'q'},
                {"help", no_argument, 0, 'h'},
                {0, 0, 0, 0}
        };

        char *options_string = "s:t:vhq";

        while ((o = getopt_long(argc, argv, options_string, long_options, &option_index)) != -1) {
                if (o == 's') {
                        if (socket_path != NULL)
                                free(socket_path);
                        socket_path = strdup(optarg);
                } else if (o == 't') {
                        if (strcasecmp(optarg, "command") == 0)
                                message_type = I3_IPC_MESSAGE_TYPE_COMMAND;
                        else if (strcasecmp(optarg, "get_workspaces") == 0)
                                message_type = I3_IPC_MESSAGE_TYPE_GET_WORKSPACES;
                        else if (strcasecmp(optarg, "get_outputs") == 0)
                                message_type = I3_IPC_MESSAGE_TYPE_GET_OUTPUTS;
                        else if (strcasecmp(optarg, "get_tree") == 0)
                                message_type = I3_IPC_MESSAGE_TYPE_GET_TREE;
                        else {
                                printf("Unknown message type\n");
                                printf("Known types: command, get_workspaces, get_outputs, get_tree\n");
                                exit(EXIT_FAILURE);
                        }
                } else if (o == 'q') {
                        quiet = true;
                } else if (o == 'v') {
                        printf("i3-msg " I3_VERSION "\n");
                        return 0;
                } else if (o == 'h') {
                        printf("i3-msg " I3_VERSION "\n");
                        printf("i3-msg [-s <socket>] [-t <type>] <message>\n");
                        return 0;
                }
        }

        if (optind < argc)
                payload = argv[optind];

        int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
        if (sockfd == -1)
                err(EXIT_FAILURE, "Could not create socket");

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_LOCAL;
        strcpy(addr.sun_path, socket_path);
        if (connect(sockfd, (const struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0)
                err(EXIT_FAILURE, "Could not connect to i3");

        ipc_send_message(sockfd, strlen(payload), message_type, (uint8_t*)payload);

        if (quiet)
                return 0;

        uint32_t reply_length;
        uint8_t *reply;
        ipc_recv_message(sockfd, message_type, &reply_length, &reply);
        printf("%.*s", reply_length, reply);
        free(reply);

        close(sockfd);

        return 0;
}
