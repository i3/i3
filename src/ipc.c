/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2011 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * ipc.c: Everything about the UNIX domain sockets for IPC
 *
 */
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <libgen.h>
#include <ev.h>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_parse.h>
#include <yajl/yajl_version.h>

#include "all.h"

char *current_socketpath = NULL;

/* Shorter names for all those yajl_gen_* functions */
#define y(x, ...) yajl_gen_ ## x (gen, ##__VA_ARGS__)
#define ystr(str) yajl_gen_string(gen, (unsigned char*)str, strlen(str))

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

/*
 * Emulates mkdir -p (creates any missing folders)
 *
 */
static bool mkdirp(const char *path) {
    if (mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0)
        return true;
    if (errno != ENOENT) {
        ELOG("mkdir(%s) failed: %s\n", path, strerror(errno));
        return false;
    }
    char *copy = strdup(path);
    /* strip trailing slashes, if any */
    while (copy[strlen(copy)-1] == '/')
        copy[strlen(copy)-1] = '\0';

    char *sep = strrchr(copy, '/');
    if (sep == NULL)
        return false;
    *sep = '\0';
    bool result = false;
    if (mkdirp(copy))
        result = mkdirp(path);
    free(copy);

    return result;
}

static void ipc_send_message(int fd, const unsigned char *payload,
                             int message_type, int message_size) {
    int buffer_size = strlen("i3-ipc") + sizeof(uint32_t) +
                      sizeof(uint32_t) + message_size;
    char msg[buffer_size];
    char *walk = msg;

    strncpy(walk, "i3-ipc", buffer_size - 1);
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
 * Sends the specified event to all IPC clients which are currently connected
 * and subscribed to this kind of event.
 *
 */
void ipc_send_event(const char *event, uint32_t message_type, const char *payload) {
    ipc_client *current;
    TAILQ_FOREACH(current, &all_clients, clients) {
        /* see if this client is interested in this event */
        bool interested = false;
        for (int i = 0; i < current->num_events; i++) {
            if (strcasecmp(current->events[i], event) != 0)
                continue;
            interested = true;
            break;
        }
        if (!interested)
            continue;

        ipc_send_message(current->fd, (const unsigned char*)payload,
                         message_type, strlen(payload));
    }
}

/*
 * Calls shutdown() on each socket and closes it. This function to be called
 * when exiting or restarting only!
 *
 */
void ipc_shutdown() {
    ipc_client *current;
    TAILQ_FOREACH(current, &all_clients, clients) {
        shutdown(current->fd, SHUT_RDWR);
        close(current->fd);
    }
}

/*
 * Executes the command and returns whether it could be successfully parsed
 * or not (at the moment, always returns true).
 *
 */
IPC_HANDLER(command) {
    /* To get a properly terminated buffer, we copy
     * message_size bytes out of the buffer */
    char *command = scalloc(message_size + 1);
    strncpy(command, (const char*)message, message_size);
    LOG("IPC: received: *%s*\n", command);
    const char *reply = parse_cmd((const char*)command);
    free(command);

    /* If no reply was provided, we just use the default success message */
    if (reply == NULL)
        reply = "{\"success\":true}";
    ipc_send_message(fd, (const unsigned char*)reply,
                     I3_IPC_REPLY_TYPE_COMMAND, strlen(reply));
}

static void dump_rect(yajl_gen gen, const char *name, Rect r) {
    ystr(name);
    y(map_open);
    ystr("x");
    y(integer, r.x);
    ystr("y");
    y(integer, r.y);
    ystr("width");
    y(integer, r.width);
    ystr("height");
    y(integer, r.height);
    y(map_close);
}

void dump_node(yajl_gen gen, struct Con *con, bool inplace_restart) {
    y(map_open);
    ystr("id");
    y(integer, (long int)con);

    ystr("type");
    y(integer, con->type);

    ystr("orientation");
    switch (con->orientation) {
        case NO_ORIENTATION:
            ystr("none");
            break;
        case HORIZ:
            ystr("horizontal");
            break;
        case VERT:
            ystr("vertical");
            break;
    }

    ystr("percent");
    y(double, con->percent);

    ystr("urgent");
    y(integer, con->urgent);

    ystr("focused");
    y(integer, (con == focused));

    ystr("layout");
    switch (con->layout) {
        case L_DEFAULT:
            ystr("default");
            break;
        case L_STACKED:
            ystr("stacked");
            break;
        case L_TABBED:
            ystr("tabbed");
            break;
        case L_DOCKAREA:
            ystr("dockarea");
            break;
        case L_OUTPUT:
            ystr("output");
            break;
    }

    ystr("border");
    switch (con->border_style) {
        case BS_NORMAL:
            ystr("normal");
            break;
        case BS_NONE:
            ystr("none");
            break;
        case BS_1PIXEL:
            ystr("1pixel");
            break;
    }

    dump_rect(gen, "rect", con->rect);
    dump_rect(gen, "window_rect", con->window_rect);
    dump_rect(gen, "geometry", con->geometry);

    ystr("name");
    ystr(con->name);

    if (con->type == CT_WORKSPACE) {
        ystr("num");
        y(integer, con->num);
    }

    ystr("window");
    if (con->window)
        y(integer, con->window->id);
    else y(null);

    ystr("nodes");
    y(array_open);
    Con *node;
    if (con->type != CT_DOCKAREA || !inplace_restart) {
        TAILQ_FOREACH(node, &(con->nodes_head), nodes) {
            dump_node(gen, node, inplace_restart);
        }
    }
    y(array_close);

    ystr("floating_nodes");
    y(array_open);
    TAILQ_FOREACH(node, &(con->floating_head), floating_windows) {
        dump_node(gen, node, inplace_restart);
    }
    y(array_close);

    ystr("focus");
    y(array_open);
    TAILQ_FOREACH(node, &(con->focus_head), nodes) {
        y(integer, (long int)node);
    }
    y(array_close);

    ystr("fullscreen_mode");
    y(integer, con->fullscreen_mode);

    ystr("swallows");
    y(array_open);
    Match *match;
    TAILQ_FOREACH(match, &(con->swallow_head), matches) {
        if (match->dock != -1) {
            y(map_open);
            ystr("dock");
            y(integer, match->dock);
            ystr("insert_where");
            y(integer, match->insert_where);
            y(map_close);
        }

        /* TODO: the other swallow keys */
    }

    if (inplace_restart) {
        if (con->window != NULL) {
            y(map_open);
            ystr("id");
            y(integer, con->window->id);
            y(map_close);
        }
    }
    y(array_close);

    y(map_close);
}

IPC_HANDLER(tree) {
    setlocale(LC_NUMERIC, "C");
#if YAJL_MAJOR >= 2
    yajl_gen gen = yajl_gen_alloc(NULL);
#else
    yajl_gen gen = yajl_gen_alloc(NULL, NULL);
#endif
    dump_node(gen, croot, false);
    setlocale(LC_NUMERIC, "");

    const unsigned char *payload;
#if YAJL_MAJOR >= 2
    size_t length;
#else
    unsigned int length;
#endif
    y(get_buf, &payload, &length);

    ipc_send_message(fd, payload, I3_IPC_REPLY_TYPE_TREE, length);
    y(free);
}

/*
 * Formats the reply message for a GET_WORKSPACES request and sends it to the
 * client
 *
 */
IPC_HANDLER(get_workspaces) {
#if YAJL_MAJOR >= 2
    yajl_gen gen = yajl_gen_alloc(NULL);
#else
    yajl_gen gen = yajl_gen_alloc(NULL, NULL);
#endif
    y(array_open);

    Con *focused_ws = con_get_workspace(focused);

    Con *output;
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        Con *ws;
        TAILQ_FOREACH(ws, &(output_get_content(output)->nodes_head), nodes) {
            assert(ws->type == CT_WORKSPACE);
            y(map_open);

            ystr("num");
            if (ws->num == -1)
                y(null);
            else y(integer, ws->num);

            ystr("name");
            ystr(ws->name);

            ystr("visible");
            y(bool, workspace_is_visible(ws));

            ystr("focused");
            y(bool, ws == focused_ws);

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
            ystr(output->name);

            ystr("urgent");
            y(bool, ws->urgent);

            y(map_close);
        }
    }

    y(array_close);

    const unsigned char *payload;
#if YAJL_MAJOR >= 2
    size_t length;
#else
    unsigned int length;
#endif
    y(get_buf, &payload, &length);

    ipc_send_message(fd, payload, I3_IPC_REPLY_TYPE_WORKSPACES, length);
    y(free);
}

/*
 * Formats the reply message for a GET_OUTPUTS request and sends it to the
 * client
 *
 */
IPC_HANDLER(get_outputs) {
#if YAJL_MAJOR >= 2
    yajl_gen gen = yajl_gen_alloc(NULL);
#else
    yajl_gen gen = yajl_gen_alloc(NULL, NULL);
#endif
    y(array_open);

    Output *output;
    TAILQ_FOREACH(output, &outputs, outputs) {
        y(map_open);

        ystr("name");
        ystr(output->name);

        ystr("active");
        y(bool, output->active);

        ystr("rect");
        y(map_open);
        ystr("x");
        y(integer, output->rect.x);
        ystr("y");
        y(integer, output->rect.y);
        ystr("width");
        y(integer, output->rect.width);
        ystr("height");
        y(integer, output->rect.height);
        y(map_close);

        ystr("current_workspace");
        Con *ws = NULL;
        if (output->con && (ws = con_get_fullscreen_con(output->con, CF_OUTPUT)))
            ystr(ws->name);
        else y(null);

        y(map_close);
    }

    y(array_close);

    const unsigned char *payload;
#if YAJL_MAJOR >= 2
    size_t length;
#else
    unsigned int length;
#endif
    y(get_buf, &payload, &length);

    ipc_send_message(fd, payload, I3_IPC_REPLY_TYPE_OUTPUTS, length);
    y(free);
}

/*
 * Callback for the YAJL parser (will be called when a string is parsed).
 *
 */
#if YAJL_MAJOR < 2
static int add_subscription(void *extra, const unsigned char *s,
                            unsigned int len) {
#else
static int add_subscription(void *extra, const unsigned char *s,
                            size_t len) {
#endif
    ipc_client *client = extra;

    DLOG("should add subscription to extra %p, sub %.*s\n", client, len, s);
    int event = client->num_events;

    client->num_events++;
    client->events = realloc(client->events, client->num_events * sizeof(char*));
    /* We copy the string because it is not null-terminated and strndup()
     * is missing on some BSD systems */
    client->events[event] = scalloc(len+1);
    memcpy(client->events[event], s, len);

    DLOG("client is now subscribed to:\n");
    for (int i = 0; i < client->num_events; i++)
        DLOG("event %s\n", client->events[i]);
    DLOG("(done)\n");

    return 1;
}

/*
 * Subscribes this connection to the event types which were given as a JSON
 * serialized array in the payload field of the message.
 *
 */
IPC_HANDLER(subscribe) {
    yajl_handle p;
    yajl_callbacks callbacks;
    yajl_status stat;
    ipc_client *current, *client = NULL;

    /* Search the ipc_client structure for this connection */
    TAILQ_FOREACH(current, &all_clients, clients) {
        if (current->fd != fd)
            continue;

        client = current;
        break;
    }

    if (client == NULL) {
        ELOG("Could not find ipc_client data structure for fd %d\n", fd);
        return;
    }

    /* Setup the JSON parser */
    memset(&callbacks, 0, sizeof(yajl_callbacks));
    callbacks.yajl_string = add_subscription;

#if YAJL_MAJOR >= 2
    p = yajl_alloc(&callbacks, NULL, (void*)client);
#else
    p = yajl_alloc(&callbacks, NULL, NULL, (void*)client);
#endif
    stat = yajl_parse(p, (const unsigned char*)message, message_size);
    if (stat != yajl_status_ok) {
        unsigned char *err;
        err = yajl_get_error(p, true, (const unsigned char*)message,
                             message_size);
        ELOG("YAJL parse error: %s\n", err);
        yajl_free_error(p, err);

        const char *reply = "{\"success\":false}";
        ipc_send_message(fd, (const unsigned char*)reply,
                         I3_IPC_REPLY_TYPE_SUBSCRIBE, strlen(reply));
        yajl_free(p);
        return;
    }
    yajl_free(p);
    const char *reply = "{\"success\":true}";
    ipc_send_message(fd, (const unsigned char*)reply,
                     I3_IPC_REPLY_TYPE_SUBSCRIBE, strlen(reply));
}

/* The index of each callback function corresponds to the numeric
 * value of the message type (see include/i3/ipc.h) */
handler_t handlers[5] = {
    handle_command,
    handle_get_workspaces,
    handle_subscribe,
    handle_get_outputs,
    handle_tree
};

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
        ipc_client *current;
        TAILQ_FOREACH(current, &all_clients, clients) {
            if (current->fd != w->fd)
                continue;

            for (int i = 0; i < current->num_events; i++)
                free(current->events[i]);
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
        uint32_t message_size;
        memcpy(&message_size, (uint32_t*)message, sizeof(uint32_t));
        message += sizeof(uint32_t);
        n -= sizeof(uint32_t);

        if (message_size > n) {
            DLOG("IPC: Either the message size was wrong or the message was not read completely, dropping\n");
            return;
        }

        /* The last 32 bits of the header are the message type */
        uint32_t message_type;
        memcpy(&message_type, (uint32_t*)message, sizeof(uint32_t));
        message += sizeof(uint32_t);
        n -= sizeof(uint32_t);

        if (message_type >= (sizeof(handlers) / sizeof(handler_t)))
            DLOG("Unhandled message type: %d\n", message_type);
        else {
            handler_t h = handlers[message_type];
            h(w->fd, message, n, message_size, message_type);
        }
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

    ipc_client *new = scalloc(sizeof(ipc_client));
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

    FREE(current_socketpath);

    char *resolved = resolve_tilde(filename);
    DLOG("Creating IPC-socket at %s\n", resolved);
    char *copy = sstrdup(resolved);
    const char *dir = dirname(copy);
    if (!path_exists(dir))
        mkdirp(dir);
    free(copy);

    /* Unlink the unix domain socket before */
    unlink(resolved);

    if ((sockfd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        free(resolved);
        return -1;
    }

    (void)fcntl(sockfd, F_SETFD, FD_CLOEXEC);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, resolved, sizeof(addr.sun_path) - 1);
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0) {
        perror("bind()");
        free(resolved);
        return -1;
    }

    set_nonblock(sockfd);

    if (listen(sockfd, 5) < 0) {
        perror("listen()");
        free(resolved);
        return -1;
    }

    current_socketpath = resolved;
    return sockfd;
}
