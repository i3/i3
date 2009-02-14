/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * (c) 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#include "i3.h"
#include "data.h"
#include "table.h"
#include "layout.h"

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
        /* Update container */
        Client *old_client = client->container->currently_focused;
        client->container->currently_focused = client;

        current_col = client->container->col;
        current_row = client->container->row;

        /* Set focus to the entered window, and flush xcb buffer immediately */
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_NONE, client->child, XCB_CURRENT_TIME);
        /* Update last/current client’s titlebar */
        if (old_client != NULL)
                decorate_window(conn, old_client);
        decorate_window(conn, client);
        xcb_flush(conn);
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
                uint32_t values[4] = {workspace->x,
                                      workspace->y,
                                      workspace->width,
                                      workspace->height};

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
