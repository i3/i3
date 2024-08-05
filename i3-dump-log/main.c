/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * i3-dump-log/main.c: Dumps the i3 SHM log to stdout.
 *
 */
#include <config.h>

#include "libi3.h"
#include "shmlog.h"

#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <i3/ipc.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

static uint32_t wrap_count;

static i3_shmlog_header *header;
static char *logbuffer,
    *walk;
static int ipcfd = -1;

static void disable_shmlog(void) {
    const char *disablecmd = "debuglog off; shmlog off";
    if (ipc_send_message(ipcfd, strlen(disablecmd),
                         I3_IPC_MESSAGE_TYPE_COMMAND, (uint8_t *)disablecmd) != 0) {
        err(EXIT_FAILURE, "IPC send");
    }

    /* Ensure the command was sent by waiting for the reply: */
    uint32_t reply_length = 0;
    uint8_t *reply = NULL;
    if (ipc_recv_message(ipcfd, I3_IPC_REPLY_TYPE_COMMAND,
                         &reply_length, &reply) != 0) {
        err(EXIT_FAILURE, "IPC recv");
    }
    free(reply);
}

static int check_for_wrap(void) {
    if (wrap_count == header->wrap_count) {
        return 0;
    }

    /* The log wrapped. Print the remaining content and reset walk to the top
     * of the log. */
    wrap_count = header->wrap_count;
    const int len = (logbuffer + header->offset_last_wrap) - walk;
    swrite(STDOUT_FILENO, walk, len);
    walk = logbuffer + sizeof(i3_shmlog_header);
    return 1;
}

static void print_till_end(void) {
    check_for_wrap();
    const int len = (logbuffer + header->offset_next_write) - walk;
    swrite(STDOUT_FILENO, walk, len);
    walk += len;
}

void errorlog(char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

int main(int argc, char *argv[]) {
    int o, option_index = 0;
    bool verbose = false;
#if !defined(__OpenBSD__)
    bool follow = false;
#endif

    static struct option long_options[] = {
        {"version", no_argument, 0, 'v'},
        {"verbose", no_argument, 0, 'V'},
#if !defined(__OpenBSD__)
        {"follow", no_argument, 0, 'f'},
#endif
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

#if !defined(__OpenBSD__)
    char *options_string = "s:vfVh";
#else
    char *options_string = "vVh";
#endif

    while ((o = getopt_long(argc, argv, options_string, long_options, &option_index)) != -1) {
        if (o == 'v') {
            printf("i3-dump-log " I3_VERSION "\n");
            return 0;
        } else if (o == 'V') {
            verbose = true;
#if !defined(__OpenBSD__)
        } else if (o == 'f') {
            follow = true;
#endif
        } else if (o == 'h') {
            printf("i3-dump-log " I3_VERSION "\n");
#if !defined(__OpenBSD__)
            printf("i3-dump-log [-fhVv]\n");
#else
            printf("i3-dump-log [-hVv]\n");
#endif
            return 0;
        }
    }

    char *shmname = root_atom_contents("I3_SHMLOG_PATH", NULL, 0);
    if (shmname == NULL) {
        /* Something failed. Let’s invest a little effort to find out what it
         * is. This is hugely helpful for users who want to debug i3 but are
         * not used to the procedure yet. */
        xcb_connection_t *conn;
        int screen;
        if ((conn = xcb_connect(NULL, &screen)) == NULL ||
            xcb_connection_has_error(conn)) {
            fprintf(stderr, "i3-dump-log: ERROR: Cannot connect to X11.\n\n");
            if (getenv("DISPLAY") == NULL) {
                fprintf(stderr, "Your DISPLAY environment variable is not set.\n");
                fprintf(stderr, "Are you running i3-dump-log via SSH or on a virtual console?\n");
                fprintf(stderr, "Try DISPLAY=:0 i3-dump-log\n");
                exit(1);
            }
            fprintf(stderr, "FYI: The DISPLAY environment variable is set to \"%s\".\n", getenv("DISPLAY"));
            exit(1);
        }
        if (root_atom_contents("I3_CONFIG_PATH", conn, screen) != NULL) {
            fprintf(stderr, "i3-dump-log: i3 is running, but SHM logging is not enabled. Enabling SHM log now while i3-dump-log is running\n\n");
            ipcfd = ipc_connect(NULL);
            const char *enablecmd = "debuglog on; shmlog 5242880";
            if (ipc_send_message(ipcfd, strlen(enablecmd),
                                 I3_IPC_MESSAGE_TYPE_COMMAND, (uint8_t *)enablecmd) != 0) {
                err(EXIT_FAILURE, "IPC send");
            }
            /* By the time we receive a reply, I3_SHMLOG_PATH is set: */
            uint32_t reply_length = 0;
            uint8_t *reply = NULL;
            if (ipc_recv_message(ipcfd, I3_IPC_REPLY_TYPE_COMMAND,
                                 &reply_length, &reply) != 0) {
                err(EXIT_FAILURE, "IPC recv");
            }
            free(reply);

            atexit(disable_shmlog);

            /* Retry: */
            shmname = root_atom_contents("I3_SHMLOG_PATH", NULL, 0);
            if (shmname == NULL && !is_debug_build()) {
                fprintf(stderr, "You seem to be using a release version of i3:\n  %s\n\n", I3_VERSION);
                fprintf(stderr, "Release versions do not use SHM logging by default,\ntherefore i3-dump-log does not work.\n\n");
                fprintf(stderr, "Please follow this guide instead:\nhttps://i3wm.org/docs/debugging-release-version.html\n");
                exit(1);
            }
        }
        if (shmname == NULL) {
            errx(EXIT_FAILURE, "Cannot get I3_SHMLOG_PATH atom contents. Is i3 running on this display?");
        }
    }

    if (*shmname == '\0') {
        errx(EXIT_FAILURE, "Cannot dump log: SHM logging is disabled in i3.");
    }

    struct stat statbuf;

    /* NB: While we must never write, we need O_RDWR for the pthread condvar. */
    int logbuffer_shm = shm_open(shmname, O_RDWR, 0);
    if (logbuffer_shm == -1) {
        err(EXIT_FAILURE, "Could not shm_open SHM segment for the i3 log (%s)", shmname);
    }

    if (fstat(logbuffer_shm, &statbuf) != 0) {
        err(EXIT_FAILURE, "stat(%s)", shmname);
    }

    logbuffer = mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, logbuffer_shm, 0);
    if (logbuffer == MAP_FAILED) {
        err(EXIT_FAILURE, "Could not mmap SHM segment for the i3 log");
    }

    header = (i3_shmlog_header *)logbuffer;

    if (verbose) {
        printf("next_write = %d, last_wrap = %d, logbuffer_size = %d, shmname = %s\n",
               header->offset_next_write, header->offset_last_wrap, header->size, shmname);
    }
    free(shmname);
    walk = logbuffer + header->offset_next_write;

    /* We first need to print old content in case there was at least one
     * wrapping already. */

    if (*walk != '\0') {
        /* In case there was a write to the buffer already, skip the first
         * old line, it very likely is mangled. Not a problem, though, the log
         * is chatty enough to have plenty lines left. */
        while (*walk != '\n') {
            walk++;
        }
        walk++;
    }

    /* In case there was no wrapping, this is a no-op, otherwise it prints the
     * old lines. */
    wrap_count = 0;
    check_for_wrap();

    /* Then start from the beginning and print the newer lines */
    walk = logbuffer + sizeof(i3_shmlog_header);
    print_till_end();

#if !defined(__OpenBSD__)
    if (!follow) {
        return 0;
    }

    char *log_stream_socket_path = root_atom_contents("I3_LOG_STREAM_SOCKET_PATH", NULL, 0);
    if (log_stream_socket_path == NULL) {
        errx(EXIT_FAILURE, "could not determine i3 log stream socket path: possible i3-dump-log and i3 version mismatch");
    }

    int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sockfd == -1) {
        err(EXIT_FAILURE, "Could not create socket");
    }

    (void)fcntl(sockfd, F_SETFD, FD_CLOEXEC);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, log_stream_socket_path, sizeof(addr.sun_path) - 1);
    if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
        err(EXIT_FAILURE, "Could not connect to i3 on socket %s", log_stream_socket_path);
    }

    /* Same size as the buffer used in log.c vlog(): */
    char buf[4096];
    for (;;) {
        const int n = read(sockfd, buf, sizeof(buf));
        if (n == -1) {
            err(EXIT_FAILURE, "read(log-stream-socket):");
        }
        if (n == 0) {
            exit(0); /* i3 closed the socket */
        }
        buf[n] = '\0';
        swrite(STDOUT_FILENO, buf, n);
    }

#endif
    exit(0);
    return 0;
}
