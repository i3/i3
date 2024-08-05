/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "libi3.h"

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/*
 * Connects to the i3 IPC socket and returns the file descriptor for the
 * socket. die()s if anything goes wrong.
 *
 */
int ipc_connect(const char *socket_path) {
    char *path = NULL;
    if (socket_path != NULL) {
        path = sstrdup(socket_path);
    }

    if (path == NULL) {
        if ((path = getenv("I3SOCK")) != NULL) {
            path = sstrdup(path);
        }
    }

    if (path == NULL) {
        path = root_atom_contents("I3_SOCKET_PATH", NULL, 0);
    }

    if (path == NULL) {
        err(EXIT_FAILURE, "Could not determine i3 socket path");
    }

    int sockfd = ipc_connect_impl(path);
    if (sockfd < 0) {
        err(EXIT_FAILURE, "Could not connect to i3 on socket %s", path);
    }
    free(path);
    return sockfd;
}

/**
 * Connects to the socket at the given path with no fallback paths. Returns
 * -1 if connect() fails and die()s for other errors.
 *
 */
int ipc_connect_impl(const char *socket_path) {
    int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sockfd == -1) {
        err(EXIT_FAILURE, "Could not create socket");
    }

    (void)fcntl(sockfd, F_SETFD, FD_CLOEXEC);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (connect(sockfd, (const struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
        close(sockfd);
        return -1;
    }
    return sockfd;
}
