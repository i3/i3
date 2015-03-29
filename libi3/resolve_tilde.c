/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2015 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */

#include "libi3.h"
#include <err.h>
#include <glob.h>
#include <stdlib.h>
#include <string.h>

/*
 * This function resolves ~ in pathnames.
 * It may resolve wildcards in the first part of the path, but if no match
 * or multiple matches are found, it just returns a copy of path as given.
 *
 */
char *resolve_tilde(const char *path) {
    static glob_t globbuf;
    char *head, *tail, *result;

    tail = strchr(path, '/');
    head = strndup(path, tail ? (size_t)(tail - path) : strlen(path));

    int res = glob(head, GLOB_TILDE, NULL, &globbuf);
    free(head);
    /* no match, or many wildcard matches are bad */
    if (res == GLOB_NOMATCH || globbuf.gl_pathc != 1)
        result = sstrdup(path);
    else if (res != 0) {
        err(EXIT_FAILURE, "glob() failed");
    } else {
        head = globbuf.gl_pathv[0];
        result = scalloc(strlen(head) + (tail ? strlen(tail) : 0) + 1);
        strncpy(result, head, strlen(head));
        if (tail)
            strncat(result, tail, strlen(tail));
    }
    globfree(&globbuf);

    return result;
}
