/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2011 Axel Wagner and contributors (see also: LICENSE)
 *
 * child.c: Getting Input for the statusline
 *
 */
#ifndef CHILD_H_
#define CHILD_H_

#define STDIN_CHUNK_SIZE 1024

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
void kill_child_at_exit();

/*
 * kill()s the child-process (if any) and closes and
 * free()s the stdin- and sigchild-watchers
 *
 */
void kill_child();

/*
 * Sends a SIGSTOP to the child-process (if existent)
 *
 */
void stop_child();

/*
 * Sends a SIGCONT to the child-process (if existent)
 *
 */
void cont_child();

#endif
