/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "libi3.h"

#include <sys/stat.h>

/*
 * Checks if the given path exists by calling stat().
 *
 */
bool path_exists(const char *path) {
    struct stat buf;
    return (stat(path, &buf) == 0);
}
