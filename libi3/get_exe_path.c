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
#include <unistd.h>

/*
 * This function returns the absolute path to the executable it is running in.
 *
 * The implementation follows https://stackoverflow.com/a/933996/712014
 *
 * Returned value must be freed by the caller.
 */
char *get_exe_path(const char *argv0) {
    size_t destpath_size = 1024;
    size_t tmp_size = 1024;
    char *destpath = smalloc(destpath_size);
    char *tmp = smalloc(tmp_size);

#if defined(__linux__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
/* Linux and Debian/kFreeBSD provide /proc/self/exe */
#if defined(__linux__) || defined(__FreeBSD_kernel__)
    const char *exepath = "/proc/self/exe";
#elif defined(__FreeBSD__)
    const char *exepath = "/proc/curproc/file";
#endif
    ssize_t linksize;

    while ((linksize = readlink(exepath, destpath, destpath_size)) == (ssize_t)destpath_size) {
        destpath_size = destpath_size * 2;
        destpath = srealloc(destpath, destpath_size);
    }
    if (linksize != -1) {
        /* readlink() does not NULL-terminate strings, so we have to. */
        destpath[linksize] = '\0';
        free(tmp);
        return destpath;
    }
#endif

    /* argv[0] is most likely a full path if it starts with a slash. */
    if (argv0[0] == '/') {
        free(tmp);
        free(destpath);
        return sstrdup(argv0);
    }

    /* if argv[0] contains a /, prepend the working directory */
    if (strchr(argv0, '/') != NULL) {
        char *retgcwd;
        while ((retgcwd = getcwd(tmp, tmp_size)) == NULL && errno == ERANGE) {
            tmp_size = tmp_size * 2;
            tmp = srealloc(tmp, tmp_size);
        }
        if (retgcwd != NULL) {
            free(destpath);
            sasprintf(&destpath, "%s/%s", tmp, argv0);
            free(tmp);
            return destpath;
        }
    }

    /* Fall back to searching $PATH (or _CS_PATH in absence of $PATH). */
    char *path = getenv("PATH");
    if (path == NULL) {
        /* _CS_PATH is typically something like "/bin:/usr/bin" */
        while (confstr(_CS_PATH, tmp, tmp_size) > tmp_size) {
            tmp_size = tmp_size * 2;
            tmp = srealloc(tmp, tmp_size);
        }
        sasprintf(&path, ":%s", tmp);
    } else {
        path = sstrdup(path);
    }
    const char *component;
    char *str = path;
    while (1) {
        if ((component = strtok(str, ":")) == NULL) {
            break;
        }
        str = NULL;
        free(destpath);
        sasprintf(&destpath, "%s/%s", component, argv0);
        /* Of course this is not 100% equivalent to actually exec()ing the
         * binary, but meh. */
        if (access(destpath, X_OK) == 0) {
            free(path);
            free(tmp);
            return destpath;
        }
    }
    free(destpath);
    free(path);
    free(tmp);

    /* Last resort: maybe it’s in /usr/bin? */
    return sstrdup("/usr/bin/i3-nagbar");
}
