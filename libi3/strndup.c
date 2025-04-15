/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "libi3.h"

#include <string.h>

#ifndef HAVE_STRNDUP
/*
 * Taken from FreeBSD
 * Returns a pointer to a new string which is a duplicate of the
 * string, but only copies at most n characters.
 *
 */
char *strndup(const char *str, size_t n) {
    size_t len;
    char *copy;

    for (len = 0; len < n && str[len]; len++) {
        continue;
    }

    copy = smalloc(len + 1);
    memcpy(copy, str, len);
    copy[len] = '\0';
    return (copy);
}
#endif
