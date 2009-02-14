#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include "i3.h"

/*
 * Starts the given application by passing it through a shell. We use double fork
 * to avoid zombie processes. As the started application’s parent exits (immediately),
 * the application is reparented to init (process-id 1), which correctly handles
 * childs, so we don’t have to do it :-).
 *
 * The shell is determined by looking for the SHELL environment variable. If it
 * does not exist, /bin/sh is used.
 *
 */
void start_application(const char *command) {
	if (fork() == 0) {
		/* Child process */
		if (fork() == 0) {
			/* Stores the path of the shell */
			static const char *shell = NULL;

			if (shell == NULL)
				if ((shell = getenv("SHELL")) == NULL)
					shell = "/bin/sh";

			/* This is the child */
			execl(shell, shell, "-c", command, NULL);
			/* not reached */
		}
		exit(0);
	}
	wait(0);
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
