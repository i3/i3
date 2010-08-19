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

char *i3_default_sock_path = "~/.i3/ipc.sock";

char *expand_path(char *path) {
    static glob_t globbuf;
    if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0) {
        printf("glob() failed");
        exit(EXIT_FAILURE);
    }
    char *result = strdup(globbuf.gl_pathc > 0 ? globbuf.gl_pathv[0] : path);
    if (result == NULL) {
        printf("malloc() failed");
        exit(EXIT_FAILURE);
    }
    globfree(&globbuf);
    return result;
}

int main(int argc, char **argv) {
    int opt;
    int option_index = 0;
    char *socket_path = NULL;
    char *command = NULL;
    char *fontname = NULL;

    static struct option long_opt[] = {
        { "socket",  required_argument, 0, 's' },
        { "command", required_argument, 0, 'c' },
        { "font",    required_argument, 0, 'f' },
        { "help",    no_argument,       0, 'h' },
        { "version", no_argument,       0, 'v' },
        { NULL,      0,                 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "s:c:f:hv", long_opt, &option_index)) != -1) {
        switch (opt) {
            case 's':
                socket_path = expand_path(optarg);
                break;
            case 'c':
                command = strdup(optarg);
                break;
            case 'f':
                fontname = strdup(optarg);
                break;
            case 'v':
                printf("i3bar version " I3BAR_VERSION " © 2010 Axel Wagner and contributors\n");
                exit(EXIT_SUCCESS);
            default:
                printf("Usage: %s [-s socket_path] [-c command] [-f font] [-h]\n", argv[0]);
                printf("-s <socket_path>: Connect to i3 via <socket_path>\n");
                printf("-c <command>: Execute <command> to get stdin\n");
                printf("-f <font>: Use X-Core-Font <font> for display\n");
                printf("-h: Display this help-message and exit\n");
                exit(EXIT_SUCCESS);
                break;
        }
    }

    if (fontname == NULL) {
        fontname = "-misc-fixed-medium-r-semicondensed--12-110-75-75-c-60-iso10646-1";
    }

    if (socket_path == NULL) {
        printf("No Socket Path Specified, default to %s\n", i3_default_sock_path);
        socket_path = expand_path(i3_default_sock_path);
    }

    main_loop = ev_default_loop(0);

    init_xcb(fontname);
    init_outputs();
    init_connection(socket_path);

    FREE(socket_path);

    subscribe_events();

    i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_OUTPUTS, NULL);
    i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_WORKSPACES, NULL);

    start_child(command);

    ev_loop(main_loop, 0);

    kill_child();

    FREE(statusline);

    clean_xcb();
    ev_default_destroy();

    free_workspaces();
    FREE_SLIST(outputs, i3_output);

    return 0;
}
