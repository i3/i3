/*
 * i3bar - an xcb-based status- and ws-bar for i3
 *
 * © 2010 Axel Wagner and contributors
 *
 * See file LICNSE for license information
 *
 */
#include <stdio.h>
#include <i3/ipc.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <ev.h>
#include <getopt.h>
#include <glob.h>

#include "common.h"

/*
 * Glob path, i.e. expand ~
 *
 */
char *expand_path(char *path) {
    static glob_t globbuf;
    if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0) {
        ELOG("glob() failed\n");
        exit(EXIT_FAILURE);
    }
    char *result = strdup(globbuf.gl_pathc > 0 ? globbuf.gl_pathv[0] : path);
    if (result == NULL) {
        ELOG("malloc() failed\n");
        exit(EXIT_FAILURE);
    }
    globfree(&globbuf);
    return result;
}

void print_usage(char *elf_name) {
    printf("Usage: %s [-s sock_path] [-c command] [-m] [-f font] [-V] [-h]\n", elf_name);
    printf("-s <sock_path>\tConnect to i3 via <sock_path>\n");
    printf("-c <command>\tExecute <command> to get stdin\n");
    printf("-m\t\tHide the bars, when mod4 is not pressed.\n");
    printf("\t\tIf -c is specified, the childprocess is sent a SIGSTOP on hiding,\n");
    printf("\t\tand a SIGCONT on unhiding of the bars\n");
    printf("-f <font>\tUse X-Core-Font <font> for display\n");
    printf("-V\t\tBe (very) verbose with the debug-output\n");
    printf("-h\t\tDisplay this help-message and exit\n");
}

int main(int argc, char **argv) {
    int opt;
    int option_index = 0;
    char *socket_path = NULL;
    char *command = NULL;
    char *fontname = NULL;
    char *i3_default_sock_path = "~/.i3/ipc.sock";

    /* Definition of the standard-config */
    config.hide_on_modifier = 0;

    static struct option long_opt[] = {
        { "socket",  required_argument, 0, 's' },
        { "command", required_argument, 0, 'c' },
        { "hide",    no_argument,       0, 'm' },
        { "font",    required_argument, 0, 'f' },
        { "help",    no_argument,       0, 'h' },
        { "version", no_argument,       0, 'v' },
        { "verbose", no_argument,       0, 'V' },
        { NULL,      0,                 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "s:c:mf:hvV", long_opt, &option_index)) != -1) {
        switch (opt) {
            case 's':
                socket_path = expand_path(optarg);
                break;
            case 'c':
                command = strdup(optarg);
                break;
            case 'm':
                config.hide_on_modifier = 1;
                break;
            case 'f':
                fontname = strdup(optarg);
                break;
            case 'v':
                printf("i3bar version " I3BAR_VERSION " © 2010 Axel Wagner and contributors\n");
                exit(EXIT_SUCCESS);
                break;
            case 'V':
                config.verbose = 1;
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
        }
    }

    if (fontname == NULL) {
        /* This is a very restrictive default. More sensefull would be something like
         * "-misc-*-*-*-*--*-*-*-*-*-*-*-*". But since that produces very ugly results
         * on my machine, let's stick with this until we have a configfile */
        fontname = "-misc-fixed-medium-r-semicondensed--12-110-75-75-c-60-iso10646-1";
    }

    if (socket_path == NULL) {
        ELOG("No Socket Path Specified, default to %s\n", i3_default_sock_path);
        socket_path = expand_path(i3_default_sock_path);
    }

    main_loop = ev_default_loop(0);

    init_xcb(fontname);
    init_outputs();
    init_connection(socket_path);

    FREE(socket_path);

    /* We subscribe to the i3-events we need */
    subscribe_events();

    /* We initiate the main-function by requesting infos about the outputs and
     * workspaces. Everything else (creating the bars, showing the right workspace-
     * buttons and more) is taken care of by the event-driveniness of the code */
    i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_OUTPUTS, NULL);
    i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_WORKSPACES, NULL);

    /* The name of this function is actually misleading. Even if no -c is specified,
     * this function initiates the watchers to listen on stdin and react accordingly */
    start_child(command);

    /* From here on everything should run smooth for itself, just start listening for
     * events. We stop simply stop the event-loop, when we are finished */
    ev_loop(main_loop, 0);

    kill_child();

    FREE(statusline);

    clean_xcb();
    ev_default_destroy();

    free_workspaces();
    FREE_SLIST(outputs, i3_output);

    return 0;
}
