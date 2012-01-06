/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2010 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * i3-msg/main.c: Utility which sends messages to a running i3-instance using
 * IPC via UNIX domain sockets.
 *
 * This (in combination with libi3/ipc_send_message.c and
 * libi3/ipc_recv_message.c) serves as an example for how to send your own
 * messages to i3.
 *
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
#include <limits.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>

#include "libi3.h"
#include <i3/ipc.h>

static char *socket_path;

int main(int argc, char *argv[]) {
    socket_path = getenv("I3SOCK");
    int o, option_index = 0;
    int message_type = I3_IPC_MESSAGE_TYPE_COMMAND;
    char *payload = NULL;
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
            socket_path = sstrdup(optarg);
        } else if (o == 't') {
            if (strcasecmp(optarg, "command") == 0)
                message_type = I3_IPC_MESSAGE_TYPE_COMMAND;
            else if (strcasecmp(optarg, "get_workspaces") == 0)
                message_type = I3_IPC_MESSAGE_TYPE_GET_WORKSPACES;
            else if (strcasecmp(optarg, "get_outputs") == 0)
                message_type = I3_IPC_MESSAGE_TYPE_GET_OUTPUTS;
            else if (strcasecmp(optarg, "get_tree") == 0)
                message_type = I3_IPC_MESSAGE_TYPE_GET_TREE;
            else if (strcasecmp(optarg, "get_marks") == 0)
                message_type = I3_IPC_MESSAGE_TYPE_GET_MARKS;
            else if (strcasecmp(optarg, "get_bar_config") == 0)
                message_type = I3_IPC_MESSAGE_TYPE_GET_BAR_CONFIG;
            else {
                printf("Unknown message type\n");
                printf("Known types: command, get_workspaces, get_outputs, get_tree, get_marks, get_bar_config\n");
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

    if (socket_path == NULL)
        socket_path = root_atom_contents("I3_SOCKET_PATH");

    /* Fall back to the default socket path */
    if (socket_path == NULL)
        socket_path = sstrdup("/tmp/i3-ipc.sock");

    /* Use all arguments, separated by whitespace, as payload.
     * This way, you don’t have to do i3-msg 'mark foo', you can use
     * i3-msg mark foo */
    while (optind < argc) {
        if (!payload) {
            payload = sstrdup(argv[optind]);
        } else {
            char *both;
            if (asprintf(&both, "%s %s", payload, argv[optind]) == -1)
                err(EXIT_FAILURE, "asprintf");
            free(payload);
            payload = both;
        }
        optind++;
    }

    if (!payload)
        payload = "";

    int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sockfd == -1)
        err(EXIT_FAILURE, "Could not create socket");

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (connect(sockfd, (const struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0)
        err(EXIT_FAILURE, "Could not connect to i3");

    if (ipc_send_message(sockfd, strlen(payload), message_type, (uint8_t*)payload) == -1)
        err(EXIT_FAILURE, "IPC: write()");

    if (quiet)
        return 0;

    uint32_t reply_length;
    uint8_t *reply;
    int ret;
    if ((ret = ipc_recv_message(sockfd, message_type, &reply_length, &reply)) != 0) {
        if (ret == -1)
            err(EXIT_FAILURE, "IPC: read()");
        exit(1);
    }
    printf("%.*s\n", reply_length, reply);
    free(reply);

    close(sockfd);

    return 0;
}
