/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * inject_randr1.5.c: An X11 proxy which interprets RandR 1.5 GetMonitors
 * requests and overwrites their reply with a custom reply.
 *
 * This tool can be refactored as necessary in order to perform the same
 * purpose for other request types. The RandR 1.5 specific portions of the code
 * have been marked as such to make such a refactoring easier.
 *
 */
#include "all.h"

#include <ev.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <libgen.h>

static void uds_connection_cb(EV_P_ ev_io *w, int revents);
static void read_client_setup_request_cb(EV_P_ ev_io *w, int revents);
static void read_server_setup_reply_cb(EV_P_ ev_io *w, int revents);
static void read_client_x11_packet_cb(EV_P_ ev_io *w, int revents);
static void read_server_x11_packet_cb(EV_P_ ev_io *w, int revents);

static char *sun_path = NULL;

void cleanup_socket(void) {
    if (sun_path != NULL) {
        unlink(sun_path);
        free(sun_path);
        sun_path = NULL;
    }
}

struct injected_reply {
    void *buf;
    off_t len;
};

/* BEGIN RandR 1.5 specific */
static struct injected_reply getmonitors_reply = {NULL, 0};
static struct injected_reply getoutputinfo_reply = {NULL, 0};
/* END RandR 1.5 specific */

#define XCB_PAD(i) (-(i)&3)

struct connstate {
    /* clientw is a libev watcher for the connection which we accept()ed. */
    ev_io *clientw;

    /* serverw is a libev watcher for the connection to X11 which we initiated
	 * on behalf of the client. */
    ev_io *serverw;

    /* sequence is the client-side sequence number counter. In X11’s wire
     * encoding, sequence counters are not included in requests, only in
     * replies. */
    int sequence;

    /* BEGIN RandR 1.5 specific */
    /* sequence number of the most recent GetExtension request for RANDR */
    int getext_randr;
    /* sequence number of the most recent RRGetMonitors request */
    int getmonitors;
    /* sequence number of the most recent RRGetOutputInfo request */
    int getoutputinfo;

    int randr_major_opcode;
    /* END RandR 1.5 specific */
};

/*
 * Returns 0 on EOF
 * Returns -1 on error (with errno from read() untouched)
 *
 */
static size_t readall_into(void *buffer, const size_t len, int fd) {
    size_t read_bytes = 0;
    while (read_bytes < len) {
        ssize_t n = read(fd, buffer + read_bytes, len - read_bytes);
        if (n <= 0) {
            return n;
        }
        read_bytes += (size_t)n;
    }
    return read_bytes;
}

/*
 * Exits the program with an error if the read failed.
 *
 */
static void must_read(int n) {
    if (n == -1) {
        err(EXIT_FAILURE, "read()");
    }
    if (n == 0) {
        errx(EXIT_FAILURE, "EOF");
    }
}

/*
 * Exits the program with an error if the write failed.
 *
 */
static void must_write(int n) {
    if (n == -1) {
        err(EXIT_FAILURE, "write()");
    }
}

static void uds_connection_cb(EV_P_ ev_io *w, int revents) {
    struct sockaddr_un addr;
    socklen_t addrlen = sizeof(addr);
    const int clientfd = accept(w->fd, (struct sockaddr *)&addr, &addrlen);
    if (clientfd == -1) {
        if (errno == EINTR) {
            return;
        }
        err(EXIT_FAILURE, "accept()");
    }

    struct connstate *connstate = scalloc(1, sizeof(struct connstate));

    ev_io *clientw = scalloc(1, sizeof(ev_io));
    connstate->clientw = clientw;
    clientw->data = connstate;
    ev_io_init(clientw, read_client_setup_request_cb, clientfd, EV_READ);
    ev_io_start(EV_A_ clientw);
}

// https://www.x.org/releases/current/doc/xproto/x11protocol.html#Encoding::Connection_Setup
static void read_client_setup_request_cb(EV_P_ ev_io *w, int revents) {
    ev_io_stop(EV_A_ w);
    struct connstate *connstate = (struct connstate *)w->data;

    /* Read X11 setup request in its entirety. */
    xcb_setup_request_t setup_request;
    must_read(readall_into(&setup_request, sizeof(setup_request), w->fd));

    /* Establish a connection to X11. */
    int fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (fd == -1) {
        err(EXIT_FAILURE, "socket()");
    }

    char *host;
    int displayp;
    if (xcb_parse_display(getenv("DISPLAY"), &host, &displayp, NULL) == 0) {
        errx(EXIT_FAILURE, "Could not parse DISPLAY=%s", getenv("DISPLAY"));
    }
    free(host);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_LOCAL;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/.X11-unix/X%d", displayp);
    if (connect(fd, (const struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        err(EXIT_FAILURE, "connect(%s)", addr.sun_path);
    }

    /* Relay setup request. */
    must_write(writeall(fd, &setup_request, sizeof(setup_request)));

    if (setup_request.authorization_protocol_name_len > 0 ||
        setup_request.authorization_protocol_data_len > 0) {
        const size_t authlen = setup_request.authorization_protocol_name_len +
                               XCB_PAD(setup_request.authorization_protocol_name_len) +
                               setup_request.authorization_protocol_data_len +
                               XCB_PAD(setup_request.authorization_protocol_data_len);
        void *buf = smalloc(authlen);
        must_read(readall_into(buf, authlen, w->fd));
        must_write(writeall(fd, buf, authlen));
        free(buf);
    }

    /* Wait for a response from the X11 server. */
    ev_io *serverw = scalloc(1, sizeof(ev_io));
    connstate->serverw = serverw;
    serverw->data = connstate;
    ev_io_init(serverw, read_server_setup_reply_cb, fd, EV_READ);
    ev_io_start(EV_A_ serverw);
}

static void read_server_setup_reply_cb(EV_P_ ev_io *w, int revents) {
    struct connstate *connstate = (struct connstate *)w->data;
    xcb_setup_failed_t setup_failed;
    must_read(readall_into(&setup_failed, sizeof(setup_failed), w->fd));

    switch (setup_failed.status) {
        case 0:
            errx(EXIT_FAILURE, "error authenticating at the X11 server");

        case 2:
            errx(EXIT_FAILURE, "two-factor auth not implemented");

        case 1:
            must_write(writeall(connstate->clientw->fd, &setup_failed, sizeof(xcb_setup_failed_t)));
            const size_t len = (setup_failed.length * 4);
            void *buf = smalloc(len);
            must_read(readall_into(buf, len, w->fd));
            must_write(writeall(connstate->clientw->fd, buf, len));
            free(buf);

            ev_set_cb(connstate->clientw, read_client_x11_packet_cb);
            ev_set_cb(connstate->serverw, read_server_x11_packet_cb);
            ev_io_start(EV_A_ connstate->clientw);
            break;

        default:
            errx(EXIT_FAILURE, "X11 protocol error: expected setup_failed.status in [0..2], got %d", setup_failed.status);
    }
}

// https://www.x.org/releases/current/doc/xproto/x11protocol.html#request_format
typedef struct {
    uint8_t opcode;
    uint8_t pad0;
    uint16_t length;
} generic_x11_request_t;

// https://www.x.org/releases/current/doc/xproto/x11protocol.html#reply_format
typedef struct {
    uint8_t code; /* if 1, this is a reply. if 0, this is an error. else, an event */
    uint8_t pad0;
    uint16_t sequence;
    uint32_t length;
} generic_x11_reply_t;

static void read_client_x11_packet_cb(EV_P_ ev_io *w, int revents) {
    struct connstate *connstate = (struct connstate *)w->data;

    void *request = smalloc(sizeof(generic_x11_request_t));
    must_read(readall_into(request, sizeof(generic_x11_request_t), connstate->clientw->fd));
    const size_t len = (((generic_x11_request_t *)request)->length * 4);
    if (len > sizeof(generic_x11_request_t)) {
        request = srealloc(request, len);
        must_read(readall_into(request + sizeof(generic_x11_request_t),
                               len - sizeof(generic_x11_request_t),
                               connstate->clientw->fd));
    }

    // XXX: sequence counter wrapping is not implemented, but should not be
    // necessary given that this tool is scoped for test cases.
    connstate->sequence++;

    /* BEGIN RandR 1.5 specific */
    const uint8_t opcode = ((generic_x11_request_t *)request)->opcode;
    if (opcode == XCB_QUERY_EXTENSION) {
        xcb_query_extension_request_t *req = request;
        const char *name = request + sizeof(xcb_query_extension_request_t);
        if (req->name_len == strlen("RANDR") &&
            strncmp(name, "RANDR", strlen("RANDR")) == 0) {
            connstate->getext_randr = connstate->sequence;
        }
    } else if (opcode == connstate->randr_major_opcode) {
        const uint8_t randr_opcode = ((generic_x11_request_t *)request)->pad0;
        if (randr_opcode == XCB_RANDR_GET_MONITORS) {
            connstate->getmonitors = connstate->sequence;
        } else if (randr_opcode == XCB_RANDR_GET_OUTPUT_INFO) {
            connstate->getoutputinfo = connstate->sequence;
        }
    }
    /* END RandR 1.5 specific */

    must_write(writeall(connstate->serverw->fd, request, len));
    free(request);
}

static bool handle_sequence(struct connstate *connstate, uint16_t sequence) {
    /* BEGIN RandR 1.5 specific */
    if (sequence == connstate->getmonitors) {
        printf("RRGetMonitors reply!\n");
        if (getmonitors_reply.buf != NULL) {
            printf("injecting reply\n");
            ((generic_x11_reply_t *)getmonitors_reply.buf)->sequence = sequence;
            must_write(writeall(connstate->clientw->fd, getmonitors_reply.buf, getmonitors_reply.len));
            return true;
        }
    }

    if (sequence == connstate->getoutputinfo) {
        printf("RRGetOutputInfo reply!\n");
        if (getoutputinfo_reply.buf != NULL) {
            printf("injecting reply\n");
            ((generic_x11_reply_t *)getoutputinfo_reply.buf)->sequence = sequence;
            must_write(writeall(connstate->clientw->fd, getoutputinfo_reply.buf, getoutputinfo_reply.len));
            return true;
        }
    }
    /* END RandR 1.5 specific */

    return false;
}

static void read_server_x11_packet_cb(EV_P_ ev_io *w, int revents) {
    struct connstate *connstate = (struct connstate *)w->data;
    // all packets from the server are at least 32 bytes in length
    size_t len = 32;
    void *packet = smalloc(len);
    must_read(readall_into(packet, len, connstate->serverw->fd));
    switch (((generic_x11_reply_t *)packet)->code) {
        case 0: {  // error
            const uint16_t sequence = ((xcb_request_error_t *)packet)->sequence;
            if (handle_sequence(connstate, sequence)) {
                free(packet);
                return;
            }
            break;
        }
        case 1:  // reply
            len += ((generic_x11_reply_t *)packet)->length * 4;
            if (len > 32) {
                packet = srealloc(packet, len);
                must_read(readall_into(packet + 32, len - 32, connstate->serverw->fd));
            }

            /* BEGIN RandR 1.5 specific */
            const uint16_t sequence = ((generic_x11_reply_t *)packet)->sequence;

            if (sequence == connstate->getext_randr) {
                xcb_query_extension_reply_t *reply = packet;
                connstate->randr_major_opcode = reply->major_opcode;
            }
            /* END RandR 1.5 specific */

            if (handle_sequence(connstate, sequence)) {
                free(packet);
                return;
            }

            break;

        default:  // event
            break;
    }
    must_write(writeall(connstate->clientw->fd, packet, len));
    free(packet);
}

static void child_cb(EV_P_ ev_child *w, int revents) {
    ev_child_stop(EV_A_ w);
    if (WIFEXITED(w->rstatus)) {
        exit(WEXITSTATUS(w->rstatus));
    } else {
        exit(WTERMSIG(w->rstatus) + 128);
    }
}

static void must_read_reply(const char *filename, struct injected_reply *reply) {
    FILE *f;
    if ((f = fopen(filename, "r")) == NULL) {
        err(EXIT_FAILURE, "fopen(%s)", filename);
    }
    struct stat stbuf;
    if (fstat(fileno(f), &stbuf) != 0) {
        err(EXIT_FAILURE, "fstat(%s)", filename);
    }
    reply->len = stbuf.st_size;
    reply->buf = smalloc(stbuf.st_size);
    int n = fread(reply->buf, 1, stbuf.st_size, f);
    if (n != stbuf.st_size) {
        err(EXIT_FAILURE, "fread(%s)", filename);
    }
    fclose(f);
}

int main(int argc, char *argv[]) {
    static struct option long_options[] = {
        {"getmonitors_reply", required_argument, 0, 0},
        {"getoutputinfo_reply", required_argument, 0, 0},
        {0, 0, 0, 0},
    };
    char *options_string = "";
    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, options_string, long_options, &option_index)) != -1) {
        switch (opt) {
            case 0: {
                const char *option_name = long_options[option_index].name;
                if (strcmp(option_name, "getmonitors_reply") == 0) {
                    must_read_reply(optarg, &getmonitors_reply);
                } else if (strcmp(option_name, "getoutputinfo_reply") == 0) {
                    must_read_reply(optarg, &getoutputinfo_reply);
                }
                break;
            }
            default:
                exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        errx(EXIT_FAILURE, "syntax: %s [options] <command>\n", argv[0]);
    }

    int fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (fd == -1) {
        err(EXIT_FAILURE, "socket(AF_UNIX)");
    }

    if (fcntl(fd, F_SETFD, FD_CLOEXEC)) {
        warn("Could not set FD_CLOEXEC");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    int i;
    bool bound = false;
    for (i = 0; i < 100; i++) {
        /* XXX: The path to X11 sockets differs on some platforms (e.g. Trusted
         * Solaris, HPUX), but since libxcb doesn’t provide a function to
         * generate the path, we’ll just have to hard-code it for now. */
        snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/.X11-unix/X%d", i);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
            warn("bind(%s)", addr.sun_path);
        } else {
            bound = true;
            /* Let the user know bind() was successful, so that they know the
             * error messages can be disregarded. */
            fprintf(stderr, "Successfuly bound to %s\n", addr.sun_path);
            sun_path = sstrdup(addr.sun_path);
            break;
        }
    }

    if (!bound) {
        err(EXIT_FAILURE, "bind()");
    }

    atexit(cleanup_socket);

    /* This program will be started for each testcase which requires it, so we
     * expect precisely one connection. */
    if (listen(fd, 1) == -1) {
        err(EXIT_FAILURE, "listen()");
    }

    pid_t child = fork();
    if (child == -1) {
        err(EXIT_FAILURE, "fork()");
    }
    if (child == 0) {
        char *display;
        sasprintf(&display, ":%d", i);
        setenv("DISPLAY", display, 1);
        free(display);

        char **child_args = argv + optind;
        execvp(child_args[0], child_args);
        err(EXIT_FAILURE, "exec()");
    }

    struct ev_loop *loop = ev_default_loop(0);

    ev_child cw;
    ev_child_init(&cw, child_cb, child, 0);
    ev_child_start(loop, &cw);

    ev_io watcher;
    ev_io_init(&watcher, uds_connection_cb, fd, EV_READ);
    ev_io_start(loop, &watcher);

    ev_run(loop, 0);
}
