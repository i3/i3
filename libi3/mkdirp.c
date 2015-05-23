#include "libi3.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/*
 * Emulates mkdir -p (creates any missing folders)
 *
 */
bool mkdirp(const char *path) {
    if (mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == 0)
        return true;
    if (errno == EEXIST) {
        struct stat st;
        /* Check that the named file actually is a directory. */
        if (stat(path, &st)) {
            ELOG("stat(%s) failed: %s\n", path, strerror(errno));
            return false;
        }
        if (!S_ISDIR(st.st_mode)) {
            ELOG("mkdir(%s) failed: %s\n", path, strerror(ENOTDIR));
            return false;
        }
        return true;
    } else if (errno != ENOENT) {
        ELOG("mkdir(%s) failed: %s\n", path, strerror(errno));
        return false;
    }
    char *copy = sstrdup(path);
    /* strip trailing slashes, if any */
    while (copy[strlen(copy) - 1] == '/')
        copy[strlen(copy) - 1] = '\0';

    char *sep = strrchr(copy, '/');
    if (sep == NULL) {
        if (copy != NULL) {
            free(copy);
            copy = NULL;
        }
        return false;
    }
    *sep = '\0';
    bool result = false;
    if (mkdirp(copy))
        result = mkdirp(path);
    free(copy);

    return result;
}
