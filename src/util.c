/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * util.c: Utility functions, which can be useful everywhere.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdarg.h>

#include "i3.h"
#include "data.h"
#include "table.h"
#include "layout.h"

int min(int a, int b) {
        return (a < b ? a : b);
}

int max(int a, int b) {
        return (a > b ? a : b);
}

/*
 * Checks if pointer is NULL and exits the whole program, printing a message to stdout
 * before with the given format (see printf())
 *
 */
void exit_if_null(void *pointer, char *fmt, ...) {
        va_list args;

        if (pointer != NULL)
                return;

        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);

        exit(EXIT_FAILURE);
}

/*
 * Prints the message (see printf()) to stderr, then exits the program.
 *
 */
void die(char *fmt, ...) {
        va_list args;

        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);

        exit(EXIT_FAILURE);
}

/*
 * The s* functions (safe) are wrappers around malloc, strdup, …, which exits if one of
 * the called functions returns NULL, meaning that there is no more memory available
 *
 */
void *smalloc(size_t size) {
        void *result = malloc(size);
        exit_if_null(result, "Too less memory for malloc(%d)\n", size);
        return result;
}

char *sstrdup(const char *str) {
        char *result = strdup(str);
        exit_if_null(result, "Too less memory for strdup()\n");
        return result;
}

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

/*
 * Sets the given client as focused by updating the data structures correctly,
 * updating the X input focus and finally re-decorating both windows (to signalize
 * the user the new focus situation)
 *
 */
void set_focus(xcb_connection_t *conn, Client *client) {
        /* TODO: check if the focus needs to be changed at all */
        /* Store current_row/current_col */
        c_ws->current_row = current_row;
        c_ws->current_col = current_col;
        c_ws = client->container->workspace;

        /* Update container */
        Client *old_client = client->container->currently_focused;
        client->container->currently_focused = client;

        current_col = client->container->col;
        current_row = client->container->row;

        /* Set focus to the entered window, and flush xcb buffer immediately */
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, client->child, XCB_CURRENT_TIME);
        //xcb_warp_pointer(conn, XCB_NONE, client->child, 0, 0, 0, 0, 10, 10);
        /* Update last/current client’s titlebar */
        if (old_client != NULL)
                decorate_window(conn, old_client);
        decorate_window(conn, client);
        xcb_flush(conn);
}

/*
 * Warps the pointer into the given client (in the middle of it, to be specific), therefore
 * selecting it
 *
 */
void warp_pointer_into(xcb_connection_t *connection, Client *client) {
        int mid_x = client->rect.width / 2,
            mid_y = client->rect.height / 2;
        xcb_warp_pointer(connection, XCB_NONE, client->child, 0, 0, 0, 0, mid_x, mid_y);
}

/*
 * Toggles fullscreen mode for the given client. It updates the data structures and
 * reconfigures (= resizes/moves) the client and its frame to the full size of the
 * screen. When leaving fullscreen, re-rendering the layout is forced.
 *
 */
void toggle_fullscreen(xcb_connection_t *conn, Client *client) {
        Workspace *workspace = client->container->workspace;

        workspace->fullscreen_client = (client->fullscreen ? NULL : client);

        client->fullscreen = !client->fullscreen;

        if (client->fullscreen) {
                printf("Entering fullscreen mode...\n");
                /* We just entered fullscreen mode, let’s configure the window */
                 uint32_t mask = XCB_CONFIG_WINDOW_X |
                                 XCB_CONFIG_WINDOW_Y |
                                 XCB_CONFIG_WINDOW_WIDTH |
                                 XCB_CONFIG_WINDOW_HEIGHT;
                uint32_t values[4] = {workspace->rect.x,
                                      workspace->rect.y,
                                      workspace->rect.width,
                                      workspace->rect.height};

                printf("child itself will be at %dx%d with size %dx%d\n",
                                values[0], values[1], values[2], values[3]);

                /* Raise the window */
                xcb_circulate_window(conn, XCB_CIRCULATE_RAISE_LOWEST, client->frame);

                xcb_configure_window(conn, client->frame, mask, values);
                xcb_configure_window(conn, client->child, mask, values);

                xcb_flush(conn);
        } else {
                printf("left fullscreen\n");
                /* Because the coordinates of the window haven’t changed, it would not be
                   re-configured if we don’t set the following flag */
                client->force_reconfigure = true;
                /* We left fullscreen mode, redraw the layout */
                render_layout(conn);
        }
}
