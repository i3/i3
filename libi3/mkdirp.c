/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "libi3.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef HAVE_MKDIRP
/*
 * Emulates mkdir -p (creates any missing folders)
 *
 */
int mkdirp(const char *path, mode_t mode) {
    if (mkdir(path, mode) == 0) {
        return 0;
    }
    if (errno == EEXIST) {
        struct stat st;
        /* Check that the named file actually is a directory. */
        if (stat(path, &st)) {
            ELOG("stat(%s) failed: %s\n", path, strerror(errno));
            return -1;
        }
        if (!S_ISDIR(st.st_mode)) {
            ELOG("mkdir(%s) failed: %s\n", path, strerror(ENOTDIR));
            return -1;
        }
        return 0;
    } else if (errno != ENOENT) {
        ELOG("mkdir(%s) failed: %s\n", path, strerror(errno));
        return -1;
    }
    char *copy = sstrdup(path);
    /* strip trailing slashes, if any */
    while (copy[strlen(copy) - 1] == '/') {
        copy[strlen(copy) - 1] = '\0';
    }

    char *sep = strrchr(copy, '/');
    if (sep == NULL) {
        free(copy);
        return -1;
    }
    *sep = '\0';
    int result = -1;
    if (mkdirp(copy, mode) == 0) {
        result = mkdirp(path, mode);
    }
    free(copy);

    return result;
}
#endif
