#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <ev.h>

#include "common.h"

ev_io    *child_io;
ev_child *child_sig;

void cleanup() {
    ev_io_stop(main_loop, child_io);
    ev_child_stop(main_loop, child_sig);
    FREE(child_io);
    FREE(child_sig);
    FREE(statusline);
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

void child_io_cb(struct ev_loop *loop, ev_io *watcher, int revents) {
    int fd = watcher->fd;
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
    printf("%s\n", buffer);
    draw_bars();
}

void child_sig_cb(struct ev_loop *loop, ev_child *watcher, int revents) {
    printf("Child (pid: %d) unexpectedly exited with status %d\n", child_pid, watcher->rstatus);
    cleanup();
}

void start_child(char *command) {
    child_pid = 0;
    if (command != NULL) {
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
                return;
            default:
                close(fd[1]);

                dup2(fd[0], STDIN_FILENO);

                break;
        }
    }

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    child_io = malloc(sizeof(ev_io));
    ev_io_init(child_io, &child_io_cb, STDIN_FILENO, EV_READ);
    ev_io_start(main_loop, child_io);

    /* We must cleanup, if the child unexpectedly terminates */
    child_sig = malloc(sizeof(ev_io));
    ev_child_init(child_sig, &child_sig_cb, child_pid, 0);
    ev_child_start(main_loop, child_sig);

}

void kill_child() {
    if (child_pid != 0) {
        kill(child_pid, SIGQUIT);
    }
    cleanup();
}
