/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2012 Axel Wagner and contributors (see also: LICENSE)
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
 * Having verboselog(), errorlog() and debuglog() is necessary when using libi3.
 *
 */
void verboselog(char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
}

void errorlog(char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void debuglog(char *fmt, ...) {
}

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
    char *result = sstrdup(globbuf.gl_pathc > 0 ? globbuf.gl_pathv[0] : path);
    globfree(&globbuf);
    return result;
}

void print_usage(char *elf_name) {
    printf("Usage: %s -b bar_id [-s sock_path] [-h] [-v]\n", elf_name);
    printf("\n");
    printf("-b, --bar_id  <bar_id>\tBar ID for which to get the configuration\n");
    printf("-s, --socket  <sock_path>\tConnect to i3 via <sock_path>\n");
    printf("-h, --help    Display this help-message and exit\n");
    printf("-v, --version Display version number and exit\n");
    printf("\n");
    printf(" PLEASE NOTE that i3bar will be automatically started by i3\n"
           " as soon as there is a 'bar' configuration block in your\n"
           " config file. You should never need to start it manually.\n");
    printf("\n");
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
    char *i3_default_sock_path = "/tmp/i3-ipc.sock";

    /* Initialize the standard config to use 0 as default */
    memset(&config, '\0', sizeof(config_t));

    static struct option long_opt[] = {
        {"socket", required_argument, 0, 's'},
        {"bar_id", required_argument, 0, 'b'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {NULL, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "b:s:hv", long_opt, &option_index)) != -1) {
        switch (opt) {
            case 's':
                socket_path = expand_path(optarg);
                break;
            case 'v':
                printf("i3bar version " I3_VERSION " © 2010-2014 Axel Wagner and contributors\n");
                exit(EXIT_SUCCESS);
                break;
            case 'b':
                config.bar_id = sstrdup(optarg);
                break;
            default:
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
                break;
        }
    }

    if (!config.bar_id) {
        /* TODO: maybe we want -f which will automatically ask i3 for the first
         * configured bar (and error out if there are too many)? */
        ELOG("No bar_id passed. Please let i3 start i3bar or specify --bar_id\n");
        exit(EXIT_FAILURE);
    }

    main_loop = ev_default_loop(0);

    char *atom_sock_path = init_xcb_early();

    if (socket_path == NULL) {
        socket_path = atom_sock_path;
    }

    if (socket_path == NULL) {
        ELOG("No Socket Path Specified, default to %s\n", i3_default_sock_path);
        socket_path = expand_path(i3_default_sock_path);
    }

    init_outputs();
    if (init_connection(socket_path)) {
        /* Request the bar configuration. When it arrives, we fill the config array. */
        i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_BAR_CONFIG, config.bar_id);
    }

    /* We listen to SIGTERM/QUIT/INT and try to exit cleanly, by stopping the main-loop.
     * We only need those watchers on the stack, so putting them on the stack saves us
     * some calls to free() */
    ev_signal *sig_term = smalloc(sizeof(ev_signal));
    ev_signal *sig_int = smalloc(sizeof(ev_signal));
    ev_signal *sig_hup = smalloc(sizeof(ev_signal));

    ev_signal_init(sig_term, &sig_cb, SIGTERM);
    ev_signal_init(sig_int, &sig_cb, SIGINT);
    ev_signal_init(sig_hup, &sig_cb, SIGHUP);

    ev_signal_start(main_loop, sig_term);
    ev_signal_start(main_loop, sig_int);
    ev_signal_start(main_loop, sig_hup);

    /* From here on everything should run smooth for itself, just start listening for
     * events. We stop simply stop the event-loop, when we are finished */
    ev_loop(main_loop, 0);

    kill_child();

    FREE(statusline_buffer);

    clean_xcb();
    ev_default_destroy();

    free_workspaces();

    return 0;
}
