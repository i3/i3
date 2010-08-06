#include <stdio.h>
#include <i3/ipc.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <ev.h>
#include <getopt.h>

#include "common.h"

char *i3_default_sock_path = "/home/mero/.i3/ipc.sock";

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
        { NULL,      0,                 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "s:c:f:h", long_opt, &option_index)) != -1) {
        switch (opt) {
            case 's':
                socket_path = malloc(strlen(optarg));
                strcpy(socket_path, optarg);
                break;
            case 'c':
                command = malloc(strlen(optarg));
                strcpy(command, optarg);
                break;
            case 'f':
                fontname = malloc(strlen(optarg));
                strcpy(socket_path, optarg);
                break;
            default:
                printf("Usage: %s [-s socket_path] [-c command] [-f font] [-h]\n", argv[0]);
                printf("-s <socket_path>: Connect to i3 via <socket_path>\n");
                printf("-c <command>: Execute <command> to get sdtin\n");
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
        socket_path = i3_default_sock_path;
    }

    main_loop = ev_default_loop(0);

    init_xcb(fontname);
    init_outputs();
    init_connection(socket_path);

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
