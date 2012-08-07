#line 2 "startup.c"
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
 * Starts the given application by passing it through a shell. We use double fork
 * to avoid zombie processes. As the started application’s parent exits (immediately),
 * the application is reparented to init (process-id 1), which correctly handles
 * childs, so we don’t have to do it :-).
 *
 * The shell is determined by looking for the SHELL environment variable. If it
 * does not exist, /bin/sh is used.
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
        if (fork() == 0) {
            /* Setup the environment variable(s) */
            if (!no_startup_id)
                sn_launcher_context_setup_child_process(context);

            /* Stores the path of the shell */
            static const char *shell = NULL;

            if (shell == NULL)
                if ((shell = getenv("SHELL")) == NULL)
                    shell = "/bin/sh";

            /* This is the child */
            execl(shell, shell, "-c", command, (void*)NULL);
            /* not reached */
        }
        _exit(0);
    }
    wait(0);

    if (!no_startup_id) {
        /* Change the pointer of the root window to indicate progress */
        if (xcursor_supported)
            xcursor_set_root_cursor(XCURSOR_CURSOR_WATCH);
        else xcb_set_root_cursor(XCURSOR_CURSOR_WATCH);
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

            /* Unref the context, will be free()d */
            sn_launcher_context_unref(sequence->context);

            /* Delete our internal sequence */
            TAILQ_REMOVE(&startup_sequences, sequence, sequences);

            if (TAILQ_EMPTY(&startup_sequences)) {
                DLOG("No more startup sequences running, changing root window cursor to default pointer.\n");
                /* Change the pointer of the root window to indicate progress */
                if (xcursor_supported)
                    xcursor_set_root_cursor(XCURSOR_CURSOR_POINTER);
                else xcb_set_root_cursor(XCURSOR_CURSOR_POINTER);
            }
            break;
        default:
            /* ignore */
            break;
    }
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
    /* The _NET_STARTUP_ID is only needed during this function, so we get it
     * here and don’t save it in the 'cwindow'. */
    if (startup_id_reply == NULL || xcb_get_property_value_length(startup_id_reply) == 0) {
        FREE(startup_id_reply);
        DLOG("No _NET_STARTUP_ID set on this window\n");
        if (cwindow->leader == XCB_NONE)
            return NULL;

        xcb_get_property_cookie_t cookie;
        cookie = xcb_get_property(conn, false, cwindow->leader, A__NET_STARTUP_ID, XCB_GET_PROPERTY_TYPE_ANY, 0, 512);
        DLOG("Checking leader window 0x%08x\n", cwindow->leader);
        startup_id_reply = xcb_get_property_reply(conn, cookie, NULL);

        if (startup_id_reply == NULL || xcb_get_property_value_length(startup_id_reply) == 0) {
            DLOG("No _NET_STARTUP_ID set on the leader either\n");
            FREE(startup_id_reply);
            return NULL;
        }
    }

    char *startup_id;
    if (asprintf(&startup_id, "%.*s", xcb_get_property_value_length(startup_id_reply),
                 (char*)xcb_get_property_value(startup_id_reply)) == -1) {
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

    free(startup_id);
    free(startup_id_reply);

    if (!sequence) {
        DLOG("WARNING: This sequence (ID %s) was not found\n", startup_id);
        return NULL;
    }

    return sequence->workspace;
}
