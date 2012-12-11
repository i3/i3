/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2012 Axel Wagner and contributors (see also: LICENSE)
 *
 * child.c: Getting Input for the statusline
 *
 */
#ifndef CHILD_H_
#define CHILD_H_

#include <stdbool.h>

#define STDIN_CHUNK_SIZE 1024

typedef struct {
    pid_t pid;

    /**
     * The version number is an uint32_t to avoid machines with different sizes of
     * 'int' to allow different values here. It’s highly unlikely we ever exceed
     * even an int8_t, but still…
     */
    uint32_t version;

    bool stopped;
    /**
     * The signal requested by the client to inform it of the hidden state of i3bar
     */
    int stop_signal;
    /**
     * The signal requested by the client to inform it of theun hidden state of i3bar
     */
    int cont_signal;
} i3bar_child;

/*
 * Start a child-process with the specified command and reroute stdin.
 * We actually start a $SHELL to execute the command so we don't have to care
 * about arguments and such
 *
 */
void start_child(char *command);

/*
 * kill()s the child-process (if any). Called when exit()ing.
 *
 */
void kill_child_at_exit(void);

/*
 * kill()s the child-process (if any) and closes and
 * free()s the stdin- and sigchild-watchers
 *
 */
void kill_child(void);

/*
 * Sends a SIGSTOP to the child-process (if existent)
 *
 */
void stop_child(void);

/*
 * Sends a SIGCONT to the child-process (if existent)
 *
 */
void cont_child(void);

#endif
