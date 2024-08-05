/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "libi3.h"

#include <unistd.h>
#include <libgen.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

/*
 * Creates the UNIX domain socket at the given path, sets it to non-blocking
 * mode, bind()s and listen()s on it.
 *
 * The full path to the socket is stored in the char* that out_socketpath points
 * to.
 *
 */
int create_socket(const char *filename, char **out_socketpath) {
    char *resolved = resolve_tilde(filename);
    DLOG("Creating UNIX socket at %s\n", resolved);
    char *copy = sstrdup(resolved);
    const char *dir = dirname(copy);
    if (!path_exists(dir)) {
        mkdirp(dir, DEFAULT_DIR_MODE);
    }
    free(copy);

    /* Check if the socket is in use by another process (this call does not
     * succeed if the socket is stale / the owner already exited) */
    int sockfd = ipc_connect_impl(resolved);
    if (sockfd != -1) {
        ELOG("Refusing to create UNIX socket at %s: Socket is already in use\n", resolved);
        close(sockfd);
        errno = EEXIST;
        return -1;
    }

    /* Unlink the unix domain socket before */
    unlink(resolved);

    sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket()");
        free(resolved);
        return -1;
    }

    (void)fcntl(sockfd, F_SETFD, FD_CLOEXEC);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_LOCAL;
    strncpy(addr.sun_path, resolved, sizeof(addr.sun_path) - 1);
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) < 0) {
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

    free(*out_socketpath);
    *out_socketpath = resolved;
    return sockfd;
}
