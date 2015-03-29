#undef I3__FILE__
#define I3__FILE__ "startup.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * startup.c: Startup notification code. Ensures a startup notification context
 *            is setup when launching applications. We store the current
 *            workspace to open windows in that startup notification context on
 *            the appropriate workspace.
 *
 */
#include "all.h"
#include "sd-daemon.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <paths.h>

#define SN_API_NOT_YET_FROZEN 1
#include <libsn/sn-launcher.h>

static TAILQ_HEAD(startup_sequence_head, Startup_Sequence) startup_sequences =
    TAILQ_HEAD_INITIALIZER(startup_sequences);

/*
 * After 60 seconds, a timeout will be triggered for each startup sequence.
 *
 * The timeout will just trigger completion of the sequence, so the normal
 * completion process takes place (startup_monitor_event will free it).
 *
 */
static void startup_timeout(EV_P_ ev_timer *w, int revents) {
    const char *id = sn_launcher_context_get_startup_id(w->data);
    DLOG("Timeout for startup sequence %s\n", id);

    struct Startup_Sequence *current, *sequence = NULL;
    TAILQ_FOREACH(current, &startup_sequences, sequences) {
        if (strcmp(current->id, id) != 0)
            continue;

        sequence = current;
        break;
    }

    /* Unref the context (for the timeout itself, see start_application) */
    sn_launcher_context_unref(w->data);

    if (!sequence) {
        DLOG("Sequence already deleted, nevermind.\n");
        return;
    }

    /* Complete the startup sequence, will trigger its deletion. */
    sn_launcher_context_complete(w->data);
    free(w);
}

/*
 * Some applications (such as Firefox) mark a startup sequence as completed
 * *before* they even map a window. Therefore, we cannot entirely delete the
 * startup sequence once it’s marked as complete. Instead, we’ll mark it for
 * deletion in 30 seconds and use that chance to delete old sequences.
 *
 * This function returns the number of active (!) startup notifications, that
 * is, those which are not marked for deletion yet. This is used for changing
 * the root window cursor.
 *
 */
static int _prune_startup_sequences(void) {
    time_t current_time = time(NULL);
    int active_sequences = 0;

    /* Traverse the list and delete everything which was marked for deletion 30
     * seconds ago or earlier. */
    struct Startup_Sequence *current, *next;
    for (next = TAILQ_FIRST(&startup_sequences);
         next != TAILQ_END(&startup_sequences);) {
        current = next;
        next = TAILQ_NEXT(next, sequences);

        if (current->delete_at == 0) {
            active_sequences++;
            continue;
        }

        if (current_time <= current->delete_at)
            continue;

        startup_sequence_delete(current);
    }

    return active_sequences;
}

/**
 * Deletes a startup sequence, ignoring whether its timeout has elapsed.
 * Useful when e.g. a window is moved between workspaces and its children
 * shouldn't spawn on the original workspace.
 *
 */
void startup_sequence_delete(struct Startup_Sequence *sequence) {
    assert(sequence != NULL);
    DLOG("Deleting startup sequence %s, delete_at = %ld, current_time = %ld\n",
         sequence->id, sequence->delete_at, time(NULL));

    /* Unref the context, will be free()d */
    sn_launcher_context_unref(sequence->context);

    /* Delete our internal sequence */
    TAILQ_REMOVE(&startup_sequences, sequence, sequences);

    free(sequence->id);
    free(sequence->workspace);
    FREE(sequence);
}

/*
 * Starts the given application by passing it through a shell. We use double fork
 * to avoid zombie processes. As the started application’s parent exits (immediately),
 * the application is reparented to init (process-id 1), which correctly handles
 * childs, so we don’t have to do it :-).
 *
 * The shell used to start applications is the system's bourne shell (i.e.,
 * /bin/sh).
 *
 * The no_startup_id flag determines whether a startup notification context
 * (and ID) should be created, which is the default and encouraged behavior.
 *
 */
void start_application(const char *command, bool no_startup_id) {
    SnLauncherContext *context;

    if (!no_startup_id) {
        /* Create a startup notification context to monitor the progress of this
         * startup. */
        context = sn_launcher_context_new(sndisplay, conn_screen);
        sn_launcher_context_set_name(context, "i3");
        sn_launcher_context_set_description(context, "exec command in i3");
        /* Chop off everything starting from the first space (if there are any
         * spaces in the command), since we don’t want the parameters. */
        char *first_word = sstrdup(command);
        char *space = strchr(first_word, ' ');
        if (space)
            *space = '\0';
        sn_launcher_context_initiate(context, "i3", first_word, last_timestamp);
        free(first_word);

        /* Trigger a timeout after 60 seconds */
        struct ev_timer *timeout = scalloc(sizeof(struct ev_timer));
        ev_timer_init(timeout, startup_timeout, 60.0, 0.);
        timeout->data = context;
        ev_timer_start(main_loop, timeout);

        LOG("startup id = %s\n", sn_launcher_context_get_startup_id(context));

        /* Save the ID and current workspace in our internal list of startup
         * sequences */
        Con *ws = con_get_workspace(focused);
        struct Startup_Sequence *sequence = scalloc(sizeof(struct Startup_Sequence));
        sequence->id = sstrdup(sn_launcher_context_get_startup_id(context));
        sequence->workspace = sstrdup(ws->name);
        sequence->context = context;
        TAILQ_INSERT_TAIL(&startup_sequences, sequence, sequences);

        /* Increase the refcount once (it starts with 1, so it will be 2 now) for
         * the timeout. Even if the sequence gets completed, the timeout still
         * needs the context (but will unref it then) */
        sn_launcher_context_ref(context);
    }

    LOG("executing: %s\n", command);
    if (fork() == 0) {
        /* Child process */
        setsid();
        setrlimit(RLIMIT_CORE, &original_rlimit_core);
        /* Close all socket activation file descriptors explicitly, we disabled
         * FD_CLOEXEC to keep them open when restarting i3. */
        for (int fd = SD_LISTEN_FDS_START;
             fd < (SD_LISTEN_FDS_START + listen_fds);
             fd++) {
            close(fd);
        }
        unsetenv("LISTEN_PID");
        unsetenv("LISTEN_FDS");
        signal(SIGPIPE, SIG_DFL);
        if (fork() == 0) {
            /* Setup the environment variable(s) */
            if (!no_startup_id)
                sn_launcher_context_setup_child_process(context);

            execl(_PATH_BSHELL, _PATH_BSHELL, "-c", command, (void *)NULL);
            /* not reached */
        }
        _exit(0);
    }
    wait(0);

    if (!no_startup_id) {
        /* Change the pointer of the root window to indicate progress */
        if (xcursor_supported)
            xcursor_set_root_cursor(XCURSOR_CURSOR_WATCH);
        else
            xcb_set_root_cursor(XCURSOR_CURSOR_WATCH);
    }
}

/*
 * Called by libstartup-notification when something happens
 *
 */
void startup_monitor_event(SnMonitorEvent *event, void *userdata) {
    SnStartupSequence *snsequence;

    snsequence = sn_monitor_event_get_startup_sequence(event);

    /* Get the corresponding internal startup sequence */
    const char *id = sn_startup_sequence_get_id(snsequence);
    struct Startup_Sequence *current, *sequence = NULL;
    TAILQ_FOREACH(current, &startup_sequences, sequences) {
        if (strcmp(current->id, id) != 0)
            continue;

        sequence = current;
        break;
    }

    if (!sequence) {
        DLOG("Got event for startup sequence that we did not initiate (ID = %s). Ignoring.\n", id);
        return;
    }

    switch (sn_monitor_event_get_type(event)) {
        case SN_MONITOR_EVENT_COMPLETED:
            DLOG("startup sequence %s completed\n", sn_startup_sequence_get_id(snsequence));

            /* Mark the given sequence for deletion in 30 seconds. */
            time_t current_time = time(NULL);
            sequence->delete_at = current_time + 30;
            DLOG("Will delete startup sequence %s at timestamp %ld\n",
                 sequence->id, sequence->delete_at);

            if (_prune_startup_sequences() == 0) {
                DLOG("No more startup sequences running, changing root window cursor to default pointer.\n");
                /* Change the pointer of the root window to indicate progress */
                if (xcursor_supported)
                    xcursor_set_root_cursor(XCURSOR_CURSOR_POINTER);
                else
                    xcb_set_root_cursor(XCURSOR_CURSOR_POINTER);
            }
            break;
        default:
            /* ignore */
            break;
    }
}

/**
 * Renames workspaces that are mentioned in the startup sequences.
 *
 */
void startup_sequence_rename_workspace(char *old_name, char *new_name) {
    struct Startup_Sequence *current;
    TAILQ_FOREACH(current, &startup_sequences, sequences) {
        if (strcmp(current->workspace, old_name) != 0)
            continue;
        DLOG("Renaming workspace \"%s\" to \"%s\" in startup sequence %s.\n",
             old_name, new_name, current->id);
        free(current->workspace);
        current->workspace = sstrdup(new_name);
    }
}

/**
 * Gets the stored startup sequence for the _NET_STARTUP_ID of a given window.
 *
 */
struct Startup_Sequence *startup_sequence_get(i3Window *cwindow,
                                              xcb_get_property_reply_t *startup_id_reply, bool ignore_mapped_leader) {
    /* The _NET_STARTUP_ID is only needed during this function, so we get it
     * here and don’t save it in the 'cwindow'. */
    if (startup_id_reply == NULL || xcb_get_property_value_length(startup_id_reply) == 0) {
        FREE(startup_id_reply);
        DLOG("No _NET_STARTUP_ID set on window 0x%08x\n", cwindow->id);
        if (cwindow->leader == XCB_NONE)
            return NULL;

        /* This is a special case that causes the leader's startup sequence
         * to only be returned if it has never been mapped, useful primarily
         * when trying to delete a sequence.
         *
         * It's generally inappropriate to delete a leader's sequence when
         * moving a child window, but if the leader has no container, it's
         * likely permanently unmapped and the child is the "real" window. */
        if (ignore_mapped_leader && con_by_window_id(cwindow->leader) != NULL) {
            DLOG("Ignoring leader window 0x%08x\n", cwindow->leader);
            return NULL;
        }

        DLOG("Checking leader window 0x%08x\n", cwindow->leader);

        xcb_get_property_cookie_t cookie;

        cookie = xcb_get_property(conn, false, cwindow->leader,
                                  A__NET_STARTUP_ID, XCB_GET_PROPERTY_TYPE_ANY, 0, 512);
        startup_id_reply = xcb_get_property_reply(conn, cookie, NULL);

        if (startup_id_reply == NULL ||
            xcb_get_property_value_length(startup_id_reply) == 0) {
            FREE(startup_id_reply);
            DLOG("No _NET_STARTUP_ID set on the leader either\n");
            return NULL;
        }
    }

    char *startup_id;
    if (asprintf(&startup_id, "%.*s", xcb_get_property_value_length(startup_id_reply),
                 (char *)xcb_get_property_value(startup_id_reply)) == -1) {
        perror("asprintf()");
        DLOG("Could not get _NET_STARTUP_ID\n");
        free(startup_id_reply);
        return NULL;
    }

    struct Startup_Sequence *current, *sequence = NULL;
    TAILQ_FOREACH(current, &startup_sequences, sequences) {
        if (strcmp(current->id, startup_id) != 0)
            continue;

        sequence = current;
        break;
    }

    if (!sequence) {
        DLOG("WARNING: This sequence (ID %s) was not found\n", startup_id);
        free(startup_id);
        free(startup_id_reply);
        return NULL;
    }

    free(startup_id);
    free(startup_id_reply);

    return sequence;
}

/*
 * Checks if the given window belongs to a startup notification by checking if
 * the _NET_STARTUP_ID property is set on the window (or on its leader, if it’s
 * unset).
 *
 * If so, returns the workspace on which the startup was initiated.
 * Returns NULL otherwise.
 *
 */
char *startup_workspace_for_window(i3Window *cwindow, xcb_get_property_reply_t *startup_id_reply) {
    struct Startup_Sequence *sequence = startup_sequence_get(cwindow, startup_id_reply, false);
    if (sequence == NULL)
        return NULL;

    /* If the startup sequence's time span has elapsed, delete it. */
    time_t current_time = time(NULL);
    if (sequence->delete_at > 0 && current_time > sequence->delete_at) {
        DLOG("Deleting expired startup sequence %s\n", sequence->id);
        startup_sequence_delete(sequence);
        return NULL;
    }

    return sequence->workspace;
}
