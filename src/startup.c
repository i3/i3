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
