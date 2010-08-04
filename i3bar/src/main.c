#include <stdio.h>
#include <i3/ipc.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <ev.h>
#include <xcb/xcb.h>

#include "ipc.h"
#include "outputs.h"
#include "workspaces.h"
#include "common.h"
#include "xcb.h"

#define STDIN_CHUNK_SIZE 1024

void ev_prepare_cb(struct ev_loop *loop, ev_prepare *w, int revents) {
    xcb_flush(xcb_connection);
}

void ev_check_cb(struct ev_loop *loop, ev_check *w, int revents) {
    xcb_generic_event_t *event;
    if ((event = xcb_poll_for_event(xcb_connection)) != NULL) {
        handle_xcb_event(event);
    }
    free(event);
}

void xcb_io_cb(struct ev_loop *loop, ev_io *w, int revents) {
}

void start_child(char *command) {
    int fd[2];
    pipe(fd);
    child_pid = fork();
    switch (child_pid) {
        case -1:
            printf("ERROR: Couldn't fork()");
            exit(EXIT_FAILURE);
        case 0:
            close(fd[0]);

            dup2(fd[1], STDOUT_FILENO);

            static const char *shell = NULL;

            if ((shell = getenv("SHELL")) == NULL)
                shell = "/bin/sh";

            execl(shell, shell, "-c", command, (char*) NULL);
            break;
        default:
            close(fd[1]);

            dup2(fd[0], STDIN_FILENO);

            break;
    }
}

void strip_dzen_formats(char *buffer) {
    char *src = buffer;
    char *dest = buffer;
    while (*src != '\0') {
        if (*src == '^') {
            if (!strncmp(src, "^ro", strlen("^ro"))) {
                *(dest++) = ' ';
                *(dest++) = '|';
                *(dest++) = ' ';
            }
            while (*src != ')') {
                src++;
            }
            src++;
        } else {
            *dest = *src;
            src++;
            dest++;
        }
    }
    *(--dest) = '\0';
}

void child_io_cb(struct ev_loop *loop, ev_io *w, int revents) {
    int fd = w->fd;
    int n = 0;
    int rec = 0;
    int buffer_len = STDIN_CHUNK_SIZE;
    char *buffer = malloc(buffer_len);
    memset(buffer, '\0', buffer_len);
    while(1) {
        n = read(fd, buffer + rec, buffer_len - rec);
        if (n == -1) {
            if (errno == EAGAIN) {
                break;
            }
            printf("ERROR: read() failed!");
            exit(EXIT_FAILURE);
        }
        if (n == 0) {
            if (rec == buffer_len) {
                char *tmp = buffer;
                buffer = malloc(buffer_len + STDIN_CHUNK_SIZE);
                memset(buffer, '\0', buffer_len);
                strncpy(buffer, tmp, buffer_len);
                buffer_len += STDIN_CHUNK_SIZE;
                FREE(tmp);
            } else {
                break;
            }
        }
        rec += n;
    }
    if (strlen(buffer) == 0) {
        FREE(buffer);
        return;
    }
    strip_dzen_formats(buffer);
    FREE(statusline);
    statusline = buffer;
    printf("%s", buffer);
    draw_bars();
}

int main(int argc, char **argv) {
    main_loop = ev_default_loop(0);

    init_xcb();
    init_connection("/home/mero/.i3/ipc.sock");

    subscribe_events();

    ev_io *xcb_io = malloc(sizeof(ev_io));
    ev_prepare *ev_prep = malloc(sizeof(ev_prepare));
    ev_check *ev_chk = malloc(sizeof(ev_check));

    ev_io_init(xcb_io, &xcb_io_cb, xcb_get_file_descriptor(xcb_connection), EV_READ);
    ev_prepare_init(ev_prep, &ev_prepare_cb);
    ev_check_init(ev_chk, &ev_check_cb);

    ev_io_start(main_loop, xcb_io);
    ev_prepare_start(main_loop, ev_prep);
    ev_check_start(main_loop, ev_chk);

    i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_OUTPUTS, NULL);
    i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_WORKSPACES, NULL);

    start_child("i3status");

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    ev_io *child_io = malloc(sizeof(ev_io));
    ev_io_init(child_io, &child_io_cb, STDIN_FILENO, EV_READ);
    ev_io_start(main_loop, child_io);

    ev_loop(main_loop, 0);

    ev_prepare_stop(main_loop, ev_prep);
    ev_check_stop(main_loop, ev_chk);
    FREE(ev_prep);
    FREE(ev_chk);

    ev_default_destroy();
    clean_xcb();

    free_workspaces();
    FREE_SLIST(outputs, i3_output);

    return 0;
}
