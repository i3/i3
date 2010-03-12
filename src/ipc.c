/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * ipc.c: Everything about the UNIX domain sockets for IPC
 *
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <ev.h>
#include <yajl/yajl_gen.h>

#include "queue.h"
#include "i3/ipc.h"
#include "i3.h"
#include "util.h"
#include "commands.h"
#include "log.h"
#include "table.h"

/* Shorter names for all those yajl_gen_* functions */
#define y(x, ...) yajl_gen_ ## x (gen, ##__VA_ARGS__)
#define ystr(str) yajl_gen_string(gen, (unsigned char*)str, strlen(str))

typedef struct ipc_client {
        int fd;

        TAILQ_ENTRY(ipc_client) clients;
} ipc_client;

TAILQ_HEAD(ipc_client_head, ipc_client) all_clients = TAILQ_HEAD_INITIALIZER(all_clients);

/*
 * Puts the given socket file descriptor into non-blocking mode or dies if
 * setting O_NONBLOCK failed. Non-blocking sockets are a good idea for our
 * IPC model because we should by no means block the window manager.
 *
 */
static void set_nonblock(int sockfd) {
        int flags = fcntl(sockfd, F_GETFL, 0);
        flags |= O_NONBLOCK;
        if (fcntl(sockfd, F_SETFL, flags) < 0)
                err(-1, "Could not set O_NONBLOCK");
}

#if 0
void broadcast(EV_P_ struct ev_timer *t, int revents) {
        ipc_client *current;
        TAILQ_FOREACH(current, &all_clients, clients) {
                write(current->fd, "hi there!\n", strlen("hi there!\n"));
        }
}
#endif

static void ipc_send_message(int fd, const unsigned char *payload,
                             int message_type, int message_size) {
        int buffer_size = strlen("i3-ipc") + sizeof(uint32_t) +
                          sizeof(uint32_t) + message_size;
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
                int n = write(fd, msg + sent_bytes, bytes_to_go);
                if (n == -1) {
                        DLOG("write() failed: %s\n", strerror(errno));
                        return;
                }

                sent_bytes += n;
                bytes_to_go -= n;
        }
}

/*
 * Formats the reply message for a GET_WORKSPACES request and sends it to the
 * client
 *
 */
static void ipc_send_workspaces(int fd) {
        Workspace *ws;

        Client *last_focused = SLIST_FIRST(&(c_ws->focus_stack));
        if (last_focused == SLIST_END(&(c_ws->focus_stack)))
                last_focused = NULL;

        yajl_gen gen = yajl_gen_alloc(NULL, NULL);
        y(array_open);

        TAILQ_FOREACH(ws, workspaces, workspaces) {
                if (ws->output == NULL)
                        continue;

                y(map_open);
                ystr("num");
                y(integer, ws->num + 1);

                ystr("name");
                ystr(ws->utf8_name);

                ystr("visible");
                y(bool, ws->output->current_workspace == ws);

                ystr("focused");
                y(bool, (last_focused != NULL && last_focused->workspace == ws));

                ystr("rect");
                y(map_open);
                ystr("x");
                y(integer, ws->rect.x);
                ystr("y");
                y(integer, ws->rect.y);
                ystr("width");
                y(integer, ws->rect.width);
                ystr("height");
                y(integer, ws->rect.height);
                y(map_close);

                ystr("output");
                ystr(ws->output->name);

                y(map_close);
        }

        y(array_close);

        const unsigned char *payload;
        unsigned int length;
        y(get_buf, &payload, &length);

        ipc_send_message(fd, payload, I3_IPC_REPLY_TYPE_WORKSPACES, length);
        y(free);
}

/*
 * Decides what to do with the received message.
 *
 * message is the raw packet, as received from the UNIX domain socket. size
 * is the remaining size of bytes for this packet.
 *
 * message_size is the size of the message as the sender specified it.
 * message_type is the type of the message as the sender specified it.
 *
 */
static void ipc_handle_message(int fd, uint8_t *message, int size,
                               uint32_t message_size, uint32_t message_type) {
        DLOG("handling message of size %d\n", size);
        DLOG("sender specified size %d\n", message_size);
        DLOG("sender specified type %d\n", message_type);
        DLOG("payload as a string = %s\n", message);

        switch (message_type) {
                case I3_IPC_MESSAGE_TYPE_COMMAND: {
                        /* To get a properly terminated buffer, we copy
                         * message_size bytes out of the buffer */
                        char *command = scalloc(message_size);
                        strncpy(command, (const char*)message, message_size);
                        parse_command(global_conn, (const char*)command);
                        free(command);

                        /* For now, every command gets a positive acknowledge
                         * (will change with the new command parser) */
                        const char *reply = "{\"success\":true}";
                        ipc_send_message(fd, (const unsigned char*)reply,
                                         I3_IPC_REPLY_TYPE_COMMAND, strlen(reply));

                        break;
                }
                case I3_IPC_MESSAGE_TYPE_GET_WORKSPACES:
                        ipc_send_workspaces(fd);
                        break;
                default:
                        DLOG("unhandled ipc message\n");
                        break;
        }
}

/*
 * Handler for activity on a client connection, receives a message from a
 * client.
 *
 * For now, the maximum message size is 2048. I’m not sure for what the
 * IPC interface will be used in the future, thus I’m not implementing a
 * mechanism for arbitrarily long messages, as it seems like overkill
 * at the moment.
 *
 */
static void ipc_receive_message(EV_P_ struct ev_io *w, int revents) {
        char buf[2048];
        int n = read(w->fd, buf, sizeof(buf));

        /* On error or an empty message, we close the connection */
        if (n <= 0) {
#if 0
                /* FIXME: I get these when closing a client socket,
                 * therefore we just treat them as an error. Is this
                 * correct? */
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                        return;
#endif

                /* If not, there was some kind of error. We don’t bother
                 * and close the connection */
                close(w->fd);

                /* Delete the client from the list of clients */
                struct ipc_client *current;
                TAILQ_FOREACH(current, &all_clients, clients) {
                        if (current->fd != w->fd)
                                continue;

                        /* We can call TAILQ_REMOVE because we break out of the
                         * TAILQ_FOREACH afterwards */
                        TAILQ_REMOVE(&all_clients, current, clients);
                        break;
                }

                ev_io_stop(EV_A_ w);

                DLOG("IPC: client disconnected\n");
                return;
        }

        /* Terminate the message correctly */
        buf[n] = '\0';

        /* Check if the message starts with the i3 IPC magic code */
        if (n < strlen(I3_IPC_MAGIC)) {
                DLOG("IPC: message too short, ignoring\n");
                return;
        }

        if (strncmp(buf, I3_IPC_MAGIC, strlen(I3_IPC_MAGIC)) != 0) {
                DLOG("IPC: message does not start with the IPC magic\n");
                return;
        }

        uint8_t *message = (uint8_t*)buf;
        while (n > 0) {
                DLOG("IPC: n = %d\n", n);
                message += strlen(I3_IPC_MAGIC);
                n -= strlen(I3_IPC_MAGIC);

                /* The next 32 bit after the magic are the message size */
                uint32_t message_size = *((uint32_t*)message);
                message += sizeof(uint32_t);
                n -= sizeof(uint32_t);

                if (message_size > n) {
                        DLOG("IPC: Either the message size was wrong or the message was not read completely, dropping\n");
                        return;
                }

                /* The last 32 bits of the header are the message type */
                uint32_t message_type = *((uint32_t*)message);
                message += sizeof(uint32_t);
                n -= sizeof(uint32_t);

                ipc_handle_message(w->fd, message, n, message_size, message_type);
                n -= message_size;
                message += message_size;
        }
}

/*
 * Handler for activity on the listening socket, meaning that a new client
 * has just connected and we should accept() him. Sets up the event handler
 * for activity on the new connection and inserts the file descriptor into
 * the list of clients.
 *
 */
void ipc_new_client(EV_P_ struct ev_io *w, int revents) {
        struct sockaddr_un peer;
        socklen_t len = sizeof(struct sockaddr_un);
        int client;
        if ((client = accept(w->fd, (struct sockaddr*)&peer, &len)) < 0) {
                if (errno == EINTR)
                        return;
                else perror("accept()");
                return;
        }

        set_nonblock(client);

        struct ev_io *package = scalloc(sizeof(struct ev_io));
        ev_io_init(package, ipc_receive_message, client, EV_READ);
        ev_io_start(EV_A_ package);

        DLOG("IPC: new client connected\n");

        struct ipc_client *new = scalloc(sizeof(struct ipc_client));
        new->fd = client;

        TAILQ_INSERT_TAIL(&all_clients, new, clients);
}

/*
 * Creates the UNIX domain socket at the given path, sets it to non-blocking
 * mode, bind()s and listen()s on it.
 *
 */
int ipc_create_socket(const char *filename) {
        int sockfd;

        /* Unlink the unix domain socket before */
        unlink(filename);

        if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
                perror("socket()");
                return -1;
        }

        (void)fcntl(sockfd, F_SETFD, FD_CLOEXEC);

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_LOCAL;
        strcpy(addr.sun_path, filename);
        if (bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0) {
                perror("bind()");
                return -1;
        }

        set_nonblock(sockfd);

        if (listen(sockfd, 5) < 0) {
                perror("listen()");
                return -1;
        }

        return sockfd;
}
