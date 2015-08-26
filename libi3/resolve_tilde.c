/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */

#include "libi3.h"
#include <err.h>
#include <stdlib.h>
#include <string.h>

/*
 * This function resolves ~ in pathnames.
 * It may resolve wildcards in the first part of the path, but if no match
 * or multiple matches are found, it just returns a copy of path as given.
 *
 */
char *resolve_tilde(const char *path) {
    char *home_dir = getenv("HOME");
    char *result, *tilde, *npath;

    tilde = strchr(path, '~');
    if (tilde == NULL) {
        return sstrdup(path);
    } else {
        result = sstrndup(path, (size_t)(tilde - path));
        result = strcat(result, home_dir);
    }

    npath = tilde + 1;
    if (strchr(npath, '~')) {
        return sstrdup(path);
    }

    return strcat(result, npath);
}
