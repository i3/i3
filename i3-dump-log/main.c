/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * i3-dump-log/main.c: Dumps the i3 SHM log to stdout.
 *
 */
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
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "libi3.h"
#include <i3/ipc.h>

int main(int argc, char *argv[]) {
    char *socket_path = getenv("I3SOCK");
    int o, option_index = 0;
    int message_type = I3_IPC_MESSAGE_TYPE_GET_LOG_MARKERS;
    bool verbose = false;

    static struct option long_options[] = {
        {"socket", required_argument, 0, 's'},
        {"version", no_argument, 0, 'v'},
        {"verbose", no_argument, 0, 'V'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    char *options_string = "s:vVh";

    while ((o = getopt_long(argc, argv, options_string, long_options, &option_index)) != -1) {
        if (o == 's') {
            if (socket_path != NULL)
                free(socket_path);
            socket_path = sstrdup(optarg);
        } else if (o == 'v') {
            printf("i3-dump-log " I3_VERSION "\n");
            return 0;
        } else if (o == 'V') {
            verbose = true;
        } else if (o == 'h') {
            printf("i3-dump-log " I3_VERSION "\n");
            printf("i3-dump-log [-s <socket>]\n");
            return 0;
        }
    }

    if (socket_path == NULL)
        socket_path = socket_path_from_x11();

    /* Fall back to the default socket path */
    if (socket_path == NULL)
        socket_path = sstrdup("/tmp/i3-ipc.sock");

    int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sockfd == -1)
        err(EXIT_FAILURE, "Could not create socket");

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (connect(sockfd, (const struct sockaddr*)&addr, sizeof(struct sockaddr_un)) < 0)
        err(EXIT_FAILURE, "Could not connect to i3");

    if (ipc_send_message(sockfd, 0, message_type, NULL) == -1)
        err(EXIT_FAILURE, "IPC: write()");

    uint32_t reply_length;
    uint8_t *reply;
    int ret;
    if ((ret = ipc_recv_message(sockfd, message_type, &reply_length, &reply)) != 0) {
        if (ret == -1)
            err(EXIT_FAILURE, "IPC: read()");
        exit(1);
    }
    char *buffer = NULL;
    sasprintf(&buffer, "%.*s", reply_length, reply);
    /* The reply will look like this:
     * {"offset_next_write":1729,"offset_last_wrap":1996,"size":2048,"shmname":"/i3-log-399"}
     * IMO, it’s not worth linking a JSON parser in just for this. If the
     * structure changes in the future, this decision needs to be re-evaluated
     * :). */
    int offset_next_write, offset_last_wrap, logbuffer_size;
    char *next_write_str = strstr(buffer, "offset_next_write"),
         *last_wrap_str = strstr(buffer, "offset_last_wrap"),
         *size_str = strstr(buffer, "size"),
         *shmname = strstr(buffer, "shmname");
    if (!next_write_str ||
        !last_wrap_str ||
        !size_str ||
        !shmname ||
        sscanf(next_write_str, "offset_next_write\":%d", &offset_next_write) != 1 ||
        sscanf(last_wrap_str, "offset_last_wrap\":%d", &offset_last_wrap) != 1 ||
        sscanf(size_str, "size\":%d", &logbuffer_size) != 1)
        errx(EXIT_FAILURE, "invalid IPC reply: %s\n", buffer);

    shmname += strlen("shmname\":\"");
    char *quote = strchr(shmname, '"');
    if (!quote)
        errx(EXIT_FAILURE, "invalid IPC reply: %s\n", buffer);
    *quote = '\0';

    if (verbose)
        printf("next_write = %d, last_wrap = %d, logbuffer_size = %d, shmname = %s\n",
               offset_next_write, offset_last_wrap, logbuffer_size, shmname);

    if (*shmname == '\0')
        errx(EXIT_FAILURE, "Cannot dump log: SHM logging is disabled in i3.");

    int logbuffer_shm = shm_open(shmname, O_RDONLY, 0);
    if (logbuffer_shm == -1)
        err(EXIT_FAILURE, "Could not shm_open SHM segment for the i3 log (%s)", shmname);

    char *logbuffer = mmap(NULL, logbuffer_size, PROT_READ, MAP_SHARED, logbuffer_shm, 0);
    if (logbuffer == MAP_FAILED)
        err(EXIT_FAILURE, "Could not mmap SHM segment for the i3 log");

    int chars;
    char *walk = logbuffer + offset_next_write;
    /* Skip the first line, it very likely is mangled. Not a problem, though,
     * the log is chatty enough to have plenty lines left. */
    while (*walk != '\0')
        walk++;

    /* Print the oldest log lines. We use printf("%s") to stop on \0. */
    while (walk < (logbuffer + offset_last_wrap)) {
        chars = printf("%s", walk);
        /* Shortcut: If there are two consecutive \0 bytes, this part of the
         * buffer was never touched. To not call printf() for every byte of the
         * buffer, we directly exit the loop. */
        if (*walk == '\0' && *(walk+1) == '\0')
            break;
        walk += (chars > 0 ? chars : 1);
    }

    /* Then start from the beginning and print the newer lines */
    walk = logbuffer;
    while (walk < (logbuffer + offset_next_write)) {
        chars = printf("%s", walk);
        walk += (chars > 0 ? chars : 1);
    }

    return 0;
}
