#undef I3__FILE__
#define I3__FILE__ "util.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * util.c: Utility functions, which can be useful everywhere within i3 (see
 *         also libi3).
 *
 */
#include "all.h"

#include <sys/wait.h>
#include <stdarg.h>
#if defined(__OpenBSD__)
#include <sys/cdefs.h>
#endif
#include <fcntl.h>
#include <pwd.h>
#include <yajl/yajl_version.h>
#include <libgen.h>
#include <ctype.h>

#define SN_API_NOT_YET_FROZEN 1
#include <libsn/sn-launcher.h>

int min(int a, int b) {
    return (a < b ? a : b);
}

int max(int a, int b) {
    return (a > b ? a : b);
}

bool rect_contains(Rect rect, uint32_t x, uint32_t y) {
    return (x >= rect.x &&
            x <= (rect.x + rect.width) &&
            y >= rect.y &&
            y <= (rect.y + rect.height));
}

Rect rect_add(Rect a, Rect b) {
    return (Rect){a.x + b.x,
                  a.y + b.y,
                  a.width + b.width,
                  a.height + b.height};
}

Rect rect_sub(Rect a, Rect b) {
    return (Rect){a.x - b.x,
                  a.y - b.y,
                  a.width - b.width,
                  a.height - b.height};
}

/*
 * Returns true if the name consists of only digits.
 *
 */
__attribute__((pure)) bool name_is_digits(const char *name) {
    /* positive integers and zero are interpreted as numbers */
    for (size_t i = 0; i < strlen(name); i++)
        if (!isdigit(name[i]))
            return false;

    return true;
}

/*
 * Parses the workspace name as a number. Returns -1 if the workspace should be
 * interpreted as a "named workspace".
 *
 */
long ws_name_to_number(const char *name) {
    /* positive integers and zero are interpreted as numbers */
    char *endptr = NULL;
    long parsed_num = strtol(name, &endptr, 10);
    if (parsed_num == LONG_MIN ||
        parsed_num == LONG_MAX ||
        parsed_num < 0 ||
        endptr == name) {
        parsed_num = -1;
    }

    return parsed_num;
}

/*
 * Updates *destination with new_value and returns true if it was changed or false
 * if it was the same
 *
 */
bool update_if_necessary(uint32_t *destination, const uint32_t new_value) {
    uint32_t old_value = *destination;

    return ((*destination = new_value) != old_value);
}

/*
 * exec()s an i3 utility, for example the config file migration script or
 * i3-nagbar. This function first searches $PATH for the given utility named,
 * then falls back to the dirname() of the i3 executable path and then falls
 * back to the dirname() of the target of /proc/self/exe (on linux).
 *
 * This function should be called after fork()ing.
 *
 * The first argument of the given argv vector will be overwritten with the
 * executable name, so pass NULL.
 *
 * If the utility cannot be found in any of these locations, it exits with
 * return code 2.
 *
 */
void exec_i3_utility(char *name, char *argv[]) {
    /* start the migration script, search PATH first */
    char *migratepath = name;
    argv[0] = migratepath;
    execvp(migratepath, argv);

    /* if the script is not in path, maybe the user installed to a strange
     * location and runs the i3 binary with an absolute path. We use
     * argv[0]’s dirname */
    char *pathbuf = strdup(start_argv[0]);
    char *dir = dirname(pathbuf);
    sasprintf(&migratepath, "%s/%s", dir, name);
    argv[0] = migratepath;
    execvp(migratepath, argv);

#if defined(__linux__)
    /* on linux, we have one more fall-back: dirname(/proc/self/exe) */
    char buffer[BUFSIZ];
    if (readlink("/proc/self/exe", buffer, BUFSIZ) == -1) {
        warn("could not read /proc/self/exe");
        _exit(1);
    }
    dir = dirname(buffer);
    sasprintf(&migratepath, "%s/%s", dir, name);
    argv[0] = migratepath;
    execvp(migratepath, argv);
#endif

    warn("Could not start %s", name);
    _exit(2);
}

/*
 * Checks a generic cookie for errors and quits with the given message if there
 * was an error.
 *
 */
void check_error(xcb_connection_t *conn, xcb_void_cookie_t cookie, char *err_message) {
    xcb_generic_error_t *error = xcb_request_check(conn, cookie);
    if (error != NULL) {
        fprintf(stderr, "ERROR: %s (X error %d)\n", err_message, error->error_code);
        xcb_disconnect(conn);
        exit(-1);
    }
}

/*
 * Checks if the given path exists by calling stat().
 *
 */
bool path_exists(const char *path) {
    struct stat buf;
    return (stat(path, &buf) == 0);
}

/*
 * Goes through the list of arguments (for exec()) and checks if the given argument
 * is present. If not, it copies the arguments (because we cannot realloc it) and
 * appends the given argument.
 *
 */
static char **append_argument(char **original, char *argument) {
    int num_args;
    for (num_args = 0; original[num_args] != NULL; num_args++) {
        DLOG("original argument: \"%s\"\n", original[num_args]);
        /* If the argument is already present we return the original pointer */
        if (strcmp(original[num_args], argument) == 0)
            return original;
    }
    /* Copy the original array */
    char **result = smalloc((num_args + 2) * sizeof(char *));
    memcpy(result, original, num_args * sizeof(char *));
    result[num_args] = argument;
    result[num_args + 1] = NULL;

    return result;
}

#define y(x, ...) yajl_gen_##x(gen, ##__VA_ARGS__)
#define ystr(str) yajl_gen_string(gen, (unsigned char *)str, strlen(str))

char *store_restart_layout(void) {
    setlocale(LC_NUMERIC, "C");
    yajl_gen gen = yajl_gen_alloc(NULL);

    dump_node(gen, croot, true);

    setlocale(LC_NUMERIC, "");

    const unsigned char *payload;
    size_t length;
    y(get_buf, &payload, &length);

    /* create a temporary file if one hasn't been specified, or just
     * resolve the tildes in the specified path */
    char *filename;
    if (config.restart_state_path == NULL) {
        filename = get_process_filename("restart-state");
        if (!filename)
            return NULL;
    } else {
        filename = resolve_tilde(config.restart_state_path);
    }

    /* create the directory, it could have been cleaned up before restarting or
     * may not exist at all in case it was user-specified. */
    char *filenamecopy = sstrdup(filename);
    char *base = dirname(filenamecopy);
    DLOG("Creating \"%s\" for storing the restart layout\n", base);
    if (!mkdirp(base))
        ELOG("Could not create \"%s\" for storing the restart layout, layout will be lost.\n", base);
    free(filenamecopy);

    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("open()");
        free(filename);
        return NULL;
    }

    if (writeall(fd, payload, length) == -1) {
        ELOG("Could not write restart layout to \"%s\", layout will be lost: %s\n", filename, strerror(errno));
        free(filename);
        close(fd);
        return NULL;
    }

    close(fd);

    if (length > 0) {
        DLOG("layout: %.*s\n", (int)length, payload);
    }

    y(free);

    return filename;
}

/*
 * Restart i3 in-place
 * appends -a to argument list to disable autostart
 *
 */
void i3_restart(bool forget_layout) {
    char *restart_filename = forget_layout ? NULL : store_restart_layout();

    kill_nagbar(&config_error_nagbar_pid, true);
    kill_nagbar(&command_error_nagbar_pid, true);

    restore_geometry();

    ipc_shutdown();

    LOG("restarting \"%s\"...\n", start_argv[0]);
    /* make sure -a is in the argument list or append it */
    start_argv = append_argument(start_argv, "-a");

    /* replace -r <file> so that the layout is restored */
    if (restart_filename != NULL) {
        /* create the new argv */
        int num_args;
        for (num_args = 0; start_argv[num_args] != NULL; num_args++)
            ;
        char **new_argv = scalloc((num_args + 3) * sizeof(char *));

        /* copy the arguments, but skip the ones we'll replace */
        int write_index = 0;
        bool skip_next = false;
        for (int i = 0; i < num_args; ++i) {
            if (skip_next)
                skip_next = false;
            else if (!strcmp(start_argv[i], "-r") ||
                     !strcmp(start_argv[i], "--restart"))
                skip_next = true;
            else
                new_argv[write_index++] = start_argv[i];
        }

        /* add the arguments we'll replace */
        new_argv[write_index++] = "--restart";
        new_argv[write_index] = restart_filename;

        /* swap the argvs */
        start_argv = new_argv;
    }

    execvp(start_argv[0], start_argv);
    /* not reached */
}

#if defined(__OpenBSD__) || defined(__APPLE__)

/*
 * Taken from FreeBSD
 * Find the first occurrence of the byte string s in byte string l.
 *
 */
void *memmem(const void *l, size_t l_len, const void *s, size_t s_len) {
    register char *cur, *last;
    const char *cl = (const char *)l;
    const char *cs = (const char *)s;

    /* we need something to compare */
    if (l_len == 0 || s_len == 0)
        return NULL;

    /* "s" must be smaller or equal to "l" */
    if (l_len < s_len)
        return NULL;

    /* special case where s_len == 1 */
    if (s_len == 1)
        return memchr(l, (int)*cs, l_len);

    /* the last position where its possible to find "s" in "l" */
    last = (char *)cl + l_len - s_len;

    for (cur = (char *)cl; cur <= last; cur++)
        if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
            return cur;

    return NULL;
}

#endif

/*
 * Handler which will be called when we get a SIGCHLD for the nagbar, meaning
 * it exited (or could not be started, depending on the exit code).
 *
 */
static void nagbar_exited(EV_P_ ev_child *watcher, int revents) {
    ev_child_stop(EV_A_ watcher);

    if (!WIFEXITED(watcher->rstatus)) {
        ELOG("ERROR: i3-nagbar did not exit normally.\n");
        return;
    }

    int exitcode = WEXITSTATUS(watcher->rstatus);
    DLOG("i3-nagbar process exited with status %d\n", exitcode);
    if (exitcode == 2) {
        ELOG("ERROR: i3-nagbar could not be found. Is it correctly installed on your system?\n");
    }

    *((pid_t *)watcher->data) = -1;
}

/*
 * Cleanup handler. Will be called when i3 exits. Kills i3-nagbar with signal
 * SIGKILL (9) to make sure there are no left-over i3-nagbar processes.
 *
 */
static void nagbar_cleanup(EV_P_ ev_cleanup *watcher, int revent) {
    pid_t *nagbar_pid = (pid_t *)watcher->data;
    if (*nagbar_pid != -1) {
        LOG("Sending SIGKILL (%d) to i3-nagbar with PID %d\n", SIGKILL, *nagbar_pid);
        kill(*nagbar_pid, SIGKILL);
    }
}

/*
 * Starts an i3-nagbar instance with the given parameters. Takes care of
 * handling SIGCHLD and killing i3-nagbar when i3 exits.
 *
 * The resulting PID will be stored in *nagbar_pid and can be used with
 * kill_nagbar() to kill the bar later on.
 *
 */
void start_nagbar(pid_t *nagbar_pid, char *argv[]) {
    if (*nagbar_pid != -1) {
        DLOG("i3-nagbar already running (PID %d), not starting again.\n", *nagbar_pid);
        return;
    }

    *nagbar_pid = fork();
    if (*nagbar_pid == -1) {
        warn("Could not fork()");
        return;
    }

    /* child */
    if (*nagbar_pid == 0)
        exec_i3_utility("i3-nagbar", argv);

    DLOG("Starting i3-nagbar with PID %d\n", *nagbar_pid);

    /* parent */
    /* install a child watcher */
    ev_child *child = smalloc(sizeof(ev_child));
    ev_child_init(child, &nagbar_exited, *nagbar_pid, 0);
    child->data = nagbar_pid;
    ev_child_start(main_loop, child);

    /* install a cleanup watcher (will be called when i3 exits and i3-nagbar is
     * still running) */
    ev_cleanup *cleanup = smalloc(sizeof(ev_cleanup));
    ev_cleanup_init(cleanup, nagbar_cleanup);
    cleanup->data = nagbar_pid;
    ev_cleanup_start(main_loop, cleanup);
}

/*
 * Kills the i3-nagbar process, if *nagbar_pid != -1.
 *
 * If wait_for_it is set (restarting i3), this function will waitpid(),
 * otherwise, ev is assumed to handle it (reloading).
 *
 */
void kill_nagbar(pid_t *nagbar_pid, bool wait_for_it) {
    if (*nagbar_pid == -1)
        return;

    if (kill(*nagbar_pid, SIGTERM) == -1)
        warn("kill(configerror_nagbar) failed");

    if (!wait_for_it)
        return;

    /* When restarting, we don’t enter the ev main loop anymore and after the
     * exec(), our old pid is no longer watched. So, ev won’t handle SIGCHLD
     * for us and we would end up with a <defunct> process. Therefore we
     * waitpid() here. */
    waitpid(*nagbar_pid, NULL, 0);
}
