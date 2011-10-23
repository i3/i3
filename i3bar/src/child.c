/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 *
 * © 2010-2011 Axel Wagner and contributors
 *
 * See file LICNSE for license information
 *
 * src/child.c: Getting Input for the statusline
 *
 */
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <ev.h>

#include "common.h"

/* Global variables for child_*() */
pid_t child_pid;

/* stdin- and sigchild-watchers */
ev_io    *stdin_io;
ev_child *child_sig;

/* The buffer statusline points to */
char *statusline_buffer = NULL;

/*
 * Stop and free() the stdin- and sigchild-watchers
 *
 */
void cleanup() {
    if (stdin_io != NULL) {
        ev_io_stop(main_loop, stdin_io);
        FREE(stdin_io);
        FREE(statusline_buffer);
	/* statusline pointed to memory within statusline_buffer */
	statusline = NULL;
    }

    if (child_sig != NULL) {
        ev_child_stop(main_loop, child_sig);
        FREE(child_sig);
    }
}

/*
 * Callbalk for stdin. We read a line from stdin and store the result
 * in statusline
 *
 */
void stdin_io_cb(struct ev_loop *loop, ev_io *watcher, int revents) {
    int fd = watcher->fd;
    int n = 0;
    int rec = 0;
    int buffer_len = STDIN_CHUNK_SIZE;
    char *buffer = smalloc(buffer_len);
    buffer[0] = '\0';
    while(1) {
        n = read(fd, buffer + rec, buffer_len - rec);
        if (n == -1) {
            if (errno == EAGAIN) {
                /* remove trailing newline and finish up */
                buffer[rec-1] = '\0';
                break;
            }
            ELOG("read() failed!: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (n == 0) {
            if (rec != 0) {
                /* remove trailing newline and finish up */
                buffer[rec-1] = '\0';
            }

            /* end of file, kill the watcher */
            ELOG("stdin: received EOF\n");
            cleanup();
            draw_bars();
            return;
        }
        rec += n;

        if (rec == buffer_len) {
            buffer_len += STDIN_CHUNK_SIZE;
            buffer = srealloc(buffer, buffer_len);
        }
    }
    if (*buffer == '\0') {
        FREE(buffer);
        return;
    }
    FREE(statusline_buffer);
    statusline = statusline_buffer = buffer;
    for (n = 0; buffer[n] != '\0'; ++n) {
        if (buffer[n] == '\n')
            statusline = &buffer[n + 1];
    }
    DLOG("%s\n", statusline);
    draw_bars();
}

/*
 * We received a sigchild, meaning, that the child-process terminated.
 * We simply free the respective data-structures and don't care for input
 * anymore
 *
 */
void child_sig_cb(struct ev_loop *loop, ev_child *watcher, int revents) {
    ELOG("Child (pid: %d) unexpectedly exited with status %d\n",
           child_pid,
           watcher->rstatus);
    cleanup();
}

/*
 * Start a child-process with the specified command and reroute stdin.
 * We actually start a $SHELL to execute the command so we don't have to care
 * about arguments and such
 *
 */
void start_child(char *command) {
    child_pid = 0;
    if (command != NULL) {
        int fd[2];
        if (pipe(fd) == -1)
            err(EXIT_FAILURE, "pipe(fd)");

        child_pid = fork();
        switch (child_pid) {
            case -1:
                ELOG("Couldn't fork(): %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            case 0:
                /* Child-process. Reroute stdout and start shell */
                close(fd[0]);

                dup2(fd[1], STDOUT_FILENO);

                static const char *shell = NULL;

                if ((shell = getenv("SHELL")) == NULL)
                    shell = "/bin/sh";

                execl(shell, shell, "-c", command, (char*) NULL);
                return;
            default:
                /* Parent-process. Rerout stdin */
                close(fd[1]);

                dup2(fd[0], STDIN_FILENO);

                /* If hide-on-modifier is set, we start of by sending the
                 * child a SIGSTOP, because the bars aren't mapped at start */
                if (config.hide_on_modifier) {
                    stop_child();
                }

                break;
        }
    }

    /* We set O_NONBLOCK because blocking is evil in event-driven software */
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    stdin_io = smalloc(sizeof(ev_io));
    ev_io_init(stdin_io, &stdin_io_cb, STDIN_FILENO, EV_READ);
    ev_io_start(main_loop, stdin_io);

    /* We must cleanup, if the child unexpectedly terminates */
    child_sig = smalloc(sizeof(ev_child));
    ev_child_init(child_sig, &child_sig_cb, child_pid, 0);
    ev_child_start(main_loop, child_sig);

}

/*
 * kill()s the child-process (if existent) and closes and
 * free()s the stdin- and sigchild-watchers
 *
 */
void kill_child() {
    if (child_pid != 0) {
        kill(child_pid, SIGCONT);
        kill(child_pid, SIGTERM);
        int status;
        waitpid(child_pid, &status, 0);
        child_pid = 0;
        cleanup();
    }
}

/*
 * Sends a SIGSTOP to the child-process (if existent)
 *
 */
void stop_child() {
    if (child_pid != 0) {
        kill(child_pid, SIGSTOP);
    }
}

/*
 * Sends a SIGCONT to the child-process (if existent)
 *
 */
void cont_child() {
    if (child_pid != 0) {
        kill(child_pid, SIGCONT);
    }
}
