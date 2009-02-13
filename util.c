#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "i3.h"

/*
 * Starts the given application with the given args.
 *
 */
void start_application(const char *path, const char *args) {
	pid_t pid;
	if ((pid = vfork()) == 0) {
		/* This is the child */
		char *argv[2];
		/* TODO: For now, we ignore args. Later on, they should be parsed
		   correctly (like in the shell?) */
		argv[0] = strdup(path);
		argv[1] = NULL;
		execve(path, argv, environment);
		/* not reached */
	}
}

/*
 * Checks a generic cookie for errors and quits with the given message if there
 * was an error.
 *
 */
void check_error(xcb_connection_t *connection, xcb_void_cookie_t cookie, char *err_message) {
	xcb_generic_error_t *error = xcb_request_check(connection, cookie);
	if (error != NULL) {
		fprintf(stderr, "ERROR: %s : %d\n", err_message , error->error_code);
		xcb_disconnect(connection);
		exit(-1);
	}
}
