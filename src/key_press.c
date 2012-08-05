/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * key_press.c: key press handler
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "all.h"

static int current_nesting_level;
static bool success_key;
static bool command_failed;

/* XXX: I don’t want to touch too much of the nagbar code at once, but we
 * should refactor this with src/cfgparse.y into a clean generic nagbar
 * interface. It might come in handy in other situations within i3, too. */
static char *pager_script_path;
static pid_t nagbar_pid = -1;

/*
 * Handler which will be called when we get a SIGCHLD for the nagbar, meaning
 * it exited (or could not be started, depending on the exit code).
 *
 */
static void nagbar_exited(EV_P_ ev_child *watcher, int revents) {
    ev_child_stop(EV_A_ watcher);

    if (unlink(pager_script_path) != 0)
        warn("Could not delete temporary i3-nagbar script %s", pager_script_path);

    if (!WIFEXITED(watcher->rstatus)) {
        fprintf(stderr, "ERROR: i3-nagbar did not exit normally.\n");
        return;
    }

    int exitcode = WEXITSTATUS(watcher->rstatus);
    printf("i3-nagbar process exited with status %d\n", exitcode);
    if (exitcode == 2) {
        fprintf(stderr, "ERROR: i3-nagbar could not be found. Is it correctly installed on your system?\n");
    }

    nagbar_pid = -1;
}

/* We need ev >= 4 for the following code. Since it is not *that* important (it
 * only makes sure that there are no i3-nagbar instances left behind) we still
 * support old systems with libev 3. */
#if EV_VERSION_MAJOR >= 4
/*
 * Cleanup handler. Will be called when i3 exits. Kills i3-nagbar with signal
 * SIGKILL (9) to make sure there are no left-over i3-nagbar processes.
 *
 */
static void nagbar_cleanup(EV_P_ ev_cleanup *watcher, int revent) {
    if (nagbar_pid != -1) {
        LOG("Sending SIGKILL (%d) to i3-nagbar with PID %d\n", SIGKILL, nagbar_pid);
        kill(nagbar_pid, SIGKILL);
    }
}
#endif

/*
 * Writes the given command as a shell script to path.
 * Returns true unless something went wrong.
 *
 */
static bool write_nagbar_script(const char *path, const char *command) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR);
    if (fd == -1) {
        warn("Could not create temporary script to store the nagbar command");
        return false;
    }
    write(fd, "#!/bin/sh\n", strlen("#!/bin/sh\n"));
    write(fd, command, strlen(command));
    close(fd);
    return true;
}

/*
 * Starts an i3-nagbar process which alerts the user that his configuration
 * file contains one or more errors. Also offers two buttons: One to launch an
 * $EDITOR on the config file and another one to launch a $PAGER on the error
 * logfile.
 *
 */
static void start_commanderror_nagbar(void) {
    if (nagbar_pid != -1) {
        DLOG("i3-nagbar for command error already running, not starting again.\n");
        return;
    }

    DLOG("Starting i3-nagbar due to command error\n");

    /* We need to create a custom script containing our actual command
     * since not every terminal emulator which is contained in
     * i3-sensible-terminal supports -e with multiple arguments (and not
     * all of them support -e with one quoted argument either).
     *
     * NB: The paths need to be unique, that is, don’t assume users close
     * their nagbars at any point in time (and they still need to work).
     * */
    pager_script_path = get_process_filename("nagbar-cfgerror-pager");

    nagbar_pid = fork();
    if (nagbar_pid == -1) {
        warn("Could not fork()");
        return;
    }

    /* child */
    if (nagbar_pid == 0) {
        char *pager_command;
        sasprintf(&pager_command, "i3-sensible-pager \"%s\"\n", errorfilename);
        if (!write_nagbar_script(pager_script_path, pager_command))
            return;

        char *pageraction;
        sasprintf(&pageraction, "i3-sensible-terminal -e \"%s\"", pager_script_path);
        char *argv[] = {
            NULL, /* will be replaced by the executable path */
            "-t",
            "error",
            "-m",
            "The configured command for this shortcut could not be run successfully.",
            "-b",
            "show errors",
            pageraction,
            NULL
        };
        exec_i3_utility("i3-nagbar", argv);
    }

    /* parent */
    /* install a child watcher */
    ev_child *child = smalloc(sizeof(ev_child));
    ev_child_init(child, &nagbar_exited, nagbar_pid, 0);
    ev_child_start(main_loop, child);

/* We need ev >= 4 for the following code. Since it is not *that* important (it
 * only makes sure that there are no i3-nagbar instances left behind) we still
 * support old systems with libev 3. */
#if EV_VERSION_MAJOR >= 4
    /* install a cleanup watcher (will be called when i3 exits and i3-nagbar is
     * still running) */
    ev_cleanup *cleanup = smalloc(sizeof(ev_cleanup));
    ev_cleanup_init(cleanup, nagbar_cleanup);
    ev_cleanup_start(main_loop, cleanup);
#endif
}

/*
 * Kills the commanderror i3-nagbar process, if any.
 *
 * Called when reloading/restarting, since the user probably fixed his wrong
 * keybindings.
 *
 * If wait_for_it is set (restarting), this function will waitpid(), otherwise,
 * ev is assumed to handle it (reloading).
 *
 */
void kill_commanderror_nagbar(bool wait_for_it) {
    if (nagbar_pid == -1)
        return;

    if (kill(nagbar_pid, SIGTERM) == -1)
        warn("kill(configerror_nagbar) failed");

    if (!wait_for_it)
        return;

    /* When restarting, we don’t enter the ev main loop anymore and after the
     * exec(), our old pid is no longer watched. So, ev won’t handle SIGCHLD
     * for us and we would end up with a <defunct> process. Therefore we
     * waitpid() here. */
    waitpid(nagbar_pid, NULL, 0);
}

static int json_boolean(void *ctx, int boolval) {
    DLOG("Got bool: %d, success_key %d, nesting_level %d\n", boolval, success_key, current_nesting_level);

    if (success_key && current_nesting_level == 1 && !boolval)
        command_failed = true;

    return 1;
}

#if YAJL_MAJOR >= 2
static int json_map_key(void *ctx, const unsigned char *stringval, size_t stringlen) {
#else
static int json_map_key(void *ctx, const unsigned char *stringval, unsigned int stringlen) {
#endif
    success_key = (stringlen >= strlen("success") &&
                   strncmp((const char*)stringval, "success", strlen("success")) == 0);
    return 1;
}

static int json_start_map(void *ctx) {
    current_nesting_level++;
    return 1;
}

static int json_end_map(void *ctx) {
    current_nesting_level--;
    return 1;
}

static yajl_callbacks command_error_callbacks = {
    NULL,
    &json_boolean,
    NULL,
    NULL,
    NULL,
    NULL,
    &json_start_map,
    &json_map_key,
    &json_end_map,
    NULL,
    NULL
};

/*
 * There was a key press. We compare this key code with our bindings table and pass
 * the bound action to parse_command().
 *
 */
void handle_key_press(xcb_key_press_event_t *event) {

    last_timestamp = event->time;

    DLOG("Keypress %d, state raw = %d\n", event->detail, event->state);

    /* Remove the numlock bit, all other bits are modifiers we can bind to */
    uint16_t state_filtered = event->state & ~(xcb_numlock_mask | XCB_MOD_MASK_LOCK);
    DLOG("(removed numlock, state = %d)\n", state_filtered);
    /* Only use the lower 8 bits of the state (modifier masks) so that mouse
     * button masks are filtered out */
    state_filtered &= 0xFF;
    DLOG("(removed upper 8 bits, state = %d)\n", state_filtered);

    if (xkb_current_group == XkbGroup2Index)
        state_filtered |= BIND_MODE_SWITCH;

    DLOG("(checked mode_switch, state %d)\n", state_filtered);

    /* Find the binding */
    Binding *bind = get_binding(state_filtered, event->detail);

    /* No match? Then the user has Mode_switch enabled but does not have a
     * specific keybinding. Fall back to the default keybindings (without
     * Mode_switch). Makes it much more convenient for users of a hybrid
     * layout (like us, ru). */
    if (bind == NULL) {
        state_filtered &= ~(BIND_MODE_SWITCH);
        DLOG("no match, new state_filtered = %d\n", state_filtered);
        if ((bind = get_binding(state_filtered, event->detail)) == NULL) {
            ELOG("Could not lookup key binding (modifiers %d, keycode %d)\n",
                 state_filtered, event->detail);
            return;
        }
    }

    char *command_copy = sstrdup(bind->command);
    struct CommandResult *command_output = parse_command(command_copy);
    free(command_copy);

    if (command_output->needs_tree_render)
        tree_render();

    /* We parse the JSON reply to figure out whether there was an error
     * ("success" being false in on of the returned dictionaries). */
    const unsigned char *reply;
#if YAJL_MAJOR >= 2
    size_t length;
    yajl_handle handle = yajl_alloc(&command_error_callbacks, NULL, NULL);
#else
    unsigned int length;
    yajl_parser_config parse_conf = { 0, 0 };

    yajl_handle handle = yajl_alloc(&command_error_callbacks, &parse_conf, NULL, NULL);
#endif
    yajl_gen_get_buf(command_output->json_gen, &reply, &length);

    current_nesting_level = 0;
    success_key = false;
    command_failed = false;
    yajl_status state = yajl_parse(handle, reply, length);
    if (state != yajl_status_ok) {
        ELOG("Could not parse my own reply. That's weird. reply is %.*s\n", (int)length, reply);
    } else {
        if (command_failed)
            start_commanderror_nagbar();
    }

    yajl_free(handle);

    yajl_gen_free(command_output->json_gen);
}
