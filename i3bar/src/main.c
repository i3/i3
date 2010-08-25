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

void print_usage(char *elf_name) {
    printf("Usage: %s [-s sock_path] [-c command] [-m] [-f font] [-h]\n", elf_name);
    printf("-s <sock_path>\tConnect to i3 via <sock_path>\n");
    printf("-c <command>\tExecute <command> to get stdin\n");
    printf("-m\t\tHide the bars, when mod4 is not pressed.\n");
    printf("\t\tIf -c is specified, the childprocess is sent a SIGSTOP on hiding,\n");
    printf("\t\tand a SIGCONT on unhiding of the bars\n");
    printf("-f <font>\tUse X-Core-Font <font> for display\n");
    printf("-h\t\tDisplay this help-message and exit\n");
}

int main(int argc, char **argv) {
    int opt;
    int option_index = 0;
    char *socket_path = NULL;
    char *command = NULL;
    char *fontname = NULL;

    /* Definition of the standard-config */
    config.hide_on_modifier = 0;

    static struct option long_opt[] = {
        { "socket",  required_argument, 0, 's' },
        { "command", required_argument, 0, 'c' },
        { "hide",    no_argument,       0, 'm' },
        { "font",    required_argument, 0, 'f' },
        { "help",    no_argument,       0, 'h' },
        { "version", no_argument,       0, 'v' },
        { NULL,      0,                 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "s:c:mf:hv", long_opt, &option_index)) != -1) {
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
            default:
                print_usage(argv[0]);
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
