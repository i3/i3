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
        ELOG("malloc() failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    globfree(&globbuf);
    return result;
}

static void read_color(char **color) {
    int len = strlen(optarg);
    if (len == 6 || (len == 7 && optarg[0] == '#')) {
        int offset = len - 6;
        int good = 1, i;
        for (i = offset; good && i < 6 + offset; ++i) {
            char c = optarg[i];
            if (!(c >= 'a' && c <= 'f')
                    && !(c >= 'A' && c <= 'F')
                    && !(c >= '0' && c <= '9')) {
                good = 0;
                break;
            }
        }
        if (good) {
            *color = strdup(optarg + offset);
            return;
        }
    }

    fprintf(stderr, "Bad color value \"%s\"\n", optarg);
    exit(EXIT_FAILURE);
}

static void free_colors(struct xcb_color_strings_t *colors) {
#define FREE_COLOR(x) \
    do { \
        if (colors->x) \
            free(colors->x); \
    } while (0)
    FREE_COLOR(bar_fg);
    FREE_COLOR(bar_bg);
    FREE_COLOR(active_ws_fg);
    FREE_COLOR(active_ws_bg);
    FREE_COLOR(inactive_ws_fg);
    FREE_COLOR(inactive_ws_bg);
    FREE_COLOR(urgent_ws_fg);
    FREE_COLOR(urgent_ws_bg);
#undef FREE_COLOR
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

/*
 * We watch various signals, that are there to make our application stop.
 * If we get one of those, we ev_unloop() and invoke the cleanup-routines
 * in main() with that
 *
 */
void sig_cb(struct ev_loop *loop, ev_signal *watcher, int revents) {
    switch (watcher->signum) {
        case SIGTERM:
            DLOG("Got a SIGTERM, stopping\n");
            break;
        case SIGINT:
            DLOG("Got a SIGINT, stopping\n");
            break;
        case SIGHUP:
            DLOG("Got a SIGHUP, stopping\n");
    }
    ev_unloop(main_loop, EVUNLOOP_ALL);
}

int main(int argc, char **argv) {
    int opt;
    int option_index = 0;
    char *socket_path = getenv("I3SOCK");
    char *command = NULL;
    char *fontname = NULL;
    char *i3_default_sock_path = "/tmp/i3-ipc.sock";
    struct xcb_color_strings_t colors = { NULL, };

    /* Definition of the standard-config */
    config.hide_on_modifier = 0;

    static struct option long_opt[] = {
        { "socket",               required_argument, 0, 's' },
        { "command",              required_argument, 0, 'c' },
        { "hide",                 no_argument,       0, 'm' },
        { "font",                 required_argument, 0, 'f' },
        { "help",                 no_argument,       0, 'h' },
        { "version",              no_argument,       0, 'v' },
        { "verbose",              no_argument,       0, 'V' },
        { "color-bar-fg",         required_argument, 0, 'A' },
        { "color-bar-bg",         required_argument, 0, 'B' },
        { "color-active-ws-fg",   required_argument, 0, 'C' },
        { "color-active-ws-bg",   required_argument, 0, 'D' },
        { "color-inactive-ws-fg", required_argument, 0, 'E' },
        { "color-inactive-ws-bg", required_argument, 0, 'F' },
        { "color-urgent-ws-bg",   required_argument, 0, 'G' },
        { "color-urgent-ws-fg",   required_argument, 0, 'H' },
        { NULL,                   0,                 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "s:c:mf:hvVA:B:C:D:E:F:G:H:", long_opt, &option_index)) != -1) {
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
            case 'A':
                read_color(&colors.bar_fg);
                break;
            case 'B':
                read_color(&colors.bar_bg);
                break;
            case 'C':
                read_color(&colors.active_ws_fg);
                break;
            case 'D':
                read_color(&colors.active_ws_bg);
                break;
            case 'E':
                read_color(&colors.inactive_ws_fg);
                break;
            case 'F':
                read_color(&colors.inactive_ws_bg);
                break;
            case 'G':
                read_color(&colors.urgent_ws_bg);
                break;
            case 'H':
                read_color(&colors.urgent_ws_fg);
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

    init_colors(&colors);
    init_xcb(fontname);

    free_colors(&colors);

    init_outputs();
    init_connection(socket_path);

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

    /* We listen to SIGTERM/QUIT/INT and try to exit cleanly, by stopping the main-loop.
     * We only need those watchers on the stack, so putting them on the stack saves us
     * some calls to free() */
    ev_signal sig_term, sig_int, sig_hup;

    ev_signal_init(&sig_term, &sig_cb, SIGTERM);
    ev_signal_init(&sig_int, &sig_cb, SIGINT);
    ev_signal_init(&sig_hup, &sig_cb, SIGHUP);

    ev_signal_start(main_loop, &sig_term);
    ev_signal_start(main_loop, &sig_int);
    ev_signal_start(main_loop, &sig_hup);

    /* From here on everything should run smooth for itself, just start listening for
     * events. We stop simply stop the event-loop, when we are finished */
    ev_loop(main_loop, 0);

    kill_child();

    FREE(statusline_buffer);

    clean_xcb();
    ev_default_destroy();

    free_workspaces();
    FREE_SLIST(outputs, i3_output);

    return 0;
}
