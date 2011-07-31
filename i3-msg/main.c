/*
 * vim:ts=4:sw=4:expandtab
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
#include <limits.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>

#include <i3/ipc.h>

static char *socket_path;

/*
 * Try to get the socket path from X11 and return NULL if it doesn’t work.
 * As i3-msg is a short-running tool, we don’t bother with cleaning up the
 * connection and leave it up to the operating system on exit.
 *
 */
static char *socket_path_from_x11() {
    xcb_connection_t *conn;
    int screen;
    if ((conn = xcb_connect(NULL, &screen)) == NULL ||
        xcb_connection_has_error(conn))
        return NULL;
    xcb_screen_t *root_screen = xcb_aux_get_screen(conn, screen);
    xcb_window_t root = root_screen->root;

    xcb_intern_atom_cookie_t atom_cookie;
    xcb_intern_atom_reply_t *atom_reply;

    atom_cookie = xcb_intern_atom(conn, 0, strlen("I3_SOCKET_PATH"), "I3_SOCKET_PATH");
    atom_reply = xcb_intern_atom_reply(conn, atom_cookie, NULL);
    if (atom_reply == NULL)
        return NULL;

    xcb_get_property_cookie_t prop_cookie;
    xcb_get_property_reply_t *prop_reply;
    prop_cookie = xcb_get_property_unchecked(conn, false, root, atom_reply->atom,
                                             XCB_GET_PROPERTY_TYPE_ANY, 0, PATH_MAX);
    prop_reply = xcb_get_property_reply(conn, prop_cookie, NULL);
    if (prop_reply == NULL || xcb_get_property_value_length(prop_reply) == 0)
        return NULL;
    if (asprintf(&socket_path, "%.*s", xcb_get_property_value_length(prop_reply),
                 (char*)xcb_get_property_value(prop_reply)) == -1)
        return NULL;
    return socket_path;
}

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

    if (socket_path == NULL)
        socket_path = socket_path_from_x11();

    /* Fall back to the default socket path */
    if (socket_path == NULL)
        socket_path = strdup("/tmp/i3-ipc.sock");

    /* Use all arguments, separated by whitespace, as payload.
     * This way, you don’t have to do i3-msg 'mark foo', you can use
     * i3-msg mark foo */
    while (optind < argc) {
        if (!payload) {
            if (!(payload = strdup(argv[optind])))
                err(EXIT_FAILURE, "strdup(argv[optind])");
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
