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
    SnStartupSequence *sequence;

    DLOG("something happened\n");
    sequence = sn_monitor_event_get_startup_sequence(event);

    switch (sn_monitor_event_get_type(event)) {
        case SN_MONITOR_EVENT_COMPLETED:
            DLOG("startup sequence %s completed\n", sn_startup_sequence_get_id(sequence));
            break;
        default:
            /* ignore */
            break;
    }
}
