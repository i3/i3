#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include "libi3.h"

/*
 * This function returns the absolute path to the executable it is running in.
 *
 * The implementation follows http://stackoverflow.com/a/933996/712014
 *
 */
const char *get_exe_path(const char *argv0) {
	static char destpath[PATH_MAX];
	char tmp[PATH_MAX];

#if defined(__linux__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
	/* Linux and Debian/kFreeBSD provide /proc/self/exe */
#if defined(__linux__) || defined(__FreeBSD_kernel__)
	const char *exepath = "/proc/self/exe";
#elif defined(__FreeBSD__)
	const char *exepath = "/proc/curproc/file";
#endif
	ssize_t linksize;

	if ((linksize = readlink(exepath, destpath, sizeof(destpath) - 1)) != -1) {
		/* readlink() does not NULL-terminate strings, so we have to. */
		destpath[linksize] = '\0';

		return destpath;
	}
#endif

	/* argv[0] is most likely a full path if it starts with a slash. */
	if (argv0[0] == '/')
		return argv0;

	/* if argv[0] contains a /, prepend the working directory */
	if (strchr(argv0, '/') != NULL &&
		getcwd(tmp, sizeof(tmp)) != NULL) {
		snprintf(destpath, sizeof(destpath), "%s/%s", tmp, argv0);
		return destpath;
	}

	/* Fall back to searching $PATH (or _CS_PATH in absence of $PATH). */
	char *path = getenv("PATH");
	if (path == NULL) {
		/* _CS_PATH is typically something like "/bin:/usr/bin" */
		confstr(_CS_PATH, tmp, sizeof(tmp));
		sasprintf(&path, ":%s", tmp);
	} else {
		path = strdup(path);
	}
	const char *component;
	char *str = path;
	while (1) {
		if ((component = strtok(str, ":")) == NULL)
			break;
		str = NULL;
		snprintf(destpath, sizeof(destpath), "%s/%s", component, argv0);
		/* Of course this is not 100% equivalent to actually exec()ing the
		 * binary, but meh. */
		if (access(destpath, X_OK) == 0) {
			free(path);
			return destpath;
		}
	}
	free(path);

	/* Last resort: maybe it’s in /usr/bin? */
	return "/usr/bin/i3-nagbar";
}
