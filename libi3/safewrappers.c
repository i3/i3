/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>

#include "libi3.h"

/*
 * The s* functions (safe) are wrappers around malloc, strdup, …, which exits if one of
 * the called functions returns NULL, meaning that there is no more memory available
 *
 */
void *smalloc(size_t size) {
    void *result = malloc(size);
    if (result == NULL)
        err(EXIT_FAILURE, "malloc(%zd)", size);
    return result;
}

void *scalloc(size_t size) {
    void *result = calloc(size, 1);
    if (result == NULL)
        err(EXIT_FAILURE, "calloc(%zd)", size);
    return result;
}

void *srealloc(void *ptr, size_t size) {
    void *result = realloc(ptr, size);
    if (result == NULL && size > 0)
        err(EXIT_FAILURE, "realloc(%zd)", size);
    return result;
}

char *sstrdup(const char *str) {
    char *result = strdup(str);
    if (result == NULL)
        err(EXIT_FAILURE, "strdup()");
    return result;
}

int sasprintf(char **strp, const char *fmt, ...) {
    va_list args;
    int result;

    va_start(args, fmt);
    if ((result = vasprintf(strp, fmt, args)) == -1)
        err(EXIT_FAILURE, "asprintf(%s)", fmt);
    va_end(args);
    return result;
}

ssize_t writeall(int fd, const void *buf, size_t count) {
    size_t written = 0;
    ssize_t n = 0;

    while (written < count) {
        n = write(fd, buf + written, count - written);
        if (n == -1) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return n;
        }
        written += (size_t)n;
    }

    return written;
}

ssize_t swrite(int fd, const void *buf, size_t count) {
    ssize_t n;

    n = writeall(fd, buf, count);
    if (n == -1)
        err(EXIT_FAILURE, "Failed to write %d", fd);
    else
        return n;
}
