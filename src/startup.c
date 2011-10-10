/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2011 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * startup.c: Startup notification code
 *
 */
#include <sys/types.h>
#include <sys/wait.h>

#define SN_API_NOT_YET_FROZEN 1
#include <libsn/sn-launcher.h>

#include "all.h"

static TAILQ_HEAD(startup_sequence_head, Startup_Sequence) startup_sequences =
    TAILQ_HEAD_INITIALIZER(startup_sequences);

/*
 * Starts the given application by passing it through a shell. We use double fork
 * to avoid zombie processes. As the started application’s parent exits (immediately),
 * the application is reparented to init (process-id 1), which correctly handles
 * childs, so we don’t have to do it :-).
 *
 * The shell is determined by looking for the SHELL environment variable. If it
 * does not exist, /bin/sh is used.
 *
 */
void start_application(const char *command) {
    /* Create a startup notification context to monitor the progress of this
     * startup. */
    SnLauncherContext *context;
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

    LOG("startup id = %s\n", sn_launcher_context_get_startup_id(context));

    /* Save the ID and current workspace in our internal list of startup
     * sequences */
    Con *ws = con_get_workspace(focused);
    struct Startup_Sequence *sequence = scalloc(sizeof(struct Startup_Sequence));
    sequence->id = sstrdup(sn_launcher_context_get_startup_id(context));
    sequence->workspace = sstrdup(ws->name);
    TAILQ_INSERT_TAIL(&startup_sequences, sequence, sequences);

    LOG("executing: %s\n", command);
    if (fork() == 0) {
        /* Child process */
        setsid();
        if (fork() == 0) {
            /* Setup the environment variable(s) */
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
        exit(0);
    }
    wait(0);
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
