/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <err.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#include <err.h>

#include "libi3.h"

/*
 * Returns the name of a temporary file with the specified prefix.
 *
 */
char *get_process_filename(const char *prefix) {
    /* dir stores the directory path for this and all subsequent calls so that
     * we only create a temporary directory once per i3 instance. */
    static char *dir = NULL;
    if (dir == NULL) {
        /* Check if XDG_RUNTIME_DIR is set. If so, we use XDG_RUNTIME_DIR/i3 */
        if ((dir = getenv("XDG_RUNTIME_DIR"))) {
            char *tmp;
            sasprintf(&tmp, "%s/i3", dir);
            dir = tmp;
            struct stat buf;
            if (stat(dir, &buf) != 0) {
                if (mkdir(dir, 0700) == -1) {
                    warn("Could not mkdir(%s)", dir);
                    errx(EXIT_FAILURE, "Check permissions of $XDG_RUNTIME_DIR = '%s'",
                         getenv("XDG_RUNTIME_DIR"));
                    perror("mkdir()");
                    return NULL;
                }
            }
        } else {
            /* If not, we create a (secure) temp directory using the template
             * /tmp/i3-<user>.XXXXXX */
            struct passwd *pw = getpwuid(getuid());
            const char *username = pw ? pw->pw_name : "unknown";
            sasprintf(&dir, "/tmp/i3-%s.XXXXXX", username);
            /* mkdtemp modifies dir */
            if (mkdtemp(dir) == NULL) {
                perror("mkdtemp()");
                return NULL;
            }
        }
    }
    char *filename;
    sasprintf(&filename, "%s/%s.%d", dir, prefix, getpid());
    return filename;
}
