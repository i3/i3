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
#include <assert.h>

#include "i3.h"
#include "data.h"
#include "table.h"
#include "layout.h"
#include "util.h"
#include "xcb.h"

int min(int a, int b) {
        return (a < b ? a : b);
}

int max(int a, int b) {
        return (a > b ? a : b);
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

void *scalloc(size_t size) {
        void *result = calloc(size, 1);
        exit_if_null(result, "Too less memory for calloc(%d)\n", size);
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
        /* The dock window cannot be focused */
        /* TODO: does this play well with dzen2’s popup menus? or do we just need to set the input
           focus but not update our internal structures? */
        if (client->dock)
                return;

        /* Store the old client */
        Client *old_client = CUR_CELL->currently_focused;

        /* TODO: check if the focus needs to be changed at all */
        /* Store current_row/current_col */
        c_ws->current_row = current_row;
        c_ws->current_col = current_col;
        c_ws = client->container->workspace;

        /* Update container */
        client->container->currently_focused = client;

        current_col = client->container->col;
        current_row = client->container->row;

        printf("set_focus(frame %08x, child %08x)\n", client->frame, client->child);
        /* Set focus to the entered window, and flush xcb buffer immediately */
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, client->child, XCB_CURRENT_TIME);
        //xcb_warp_pointer(conn, XCB_NONE, client->child, 0, 0, 0, 0, 10, 10);

        /* If we’re in stacking mode, this renders the container to update changes in the title
           bars and to raise the focused client */
        if ((old_client != NULL) && (old_client != client))
                redecorate_window(conn, old_client);

        /* redecorate_window flushes, so we don’t need to */
        redecorate_window(conn, client);
}

/*
 * Switches the layout of the given container taking care of the necessary house-keeping
 *
 */
void switch_layout_mode(xcb_connection_t *conn, Container *container, int mode) {
        if (mode == MODE_STACK) {
                /* When we’re already in stacking mode, nothing has to be done */
                if (container->mode == MODE_STACK)
                        return;

                /* When entering stacking mode, we need to open a window on which we can draw the
                   title bars of the clients, it has height 1 because we don’t bother here with
                   calculating the correct height - it will be adjusted when rendering anyways. */
                Rect rect = {container->x, container->y, container->width, 1 };

                uint32_t mask = 0;
                uint32_t values[2];

                /* Don’t generate events for our new window, it should *not* be managed */
                mask |= XCB_CW_OVERRIDE_REDIRECT;
                values[0] = 1;

                /* We want to know when… */
                mask |= XCB_CW_EVENT_MASK;
                values[1] =     XCB_EVENT_MASK_BUTTON_PRESS |   /* …mouse is pressed */
                                XCB_EVENT_MASK_EXPOSURE;        /* …our window needs to be redrawn */

                struct Stack_Window *stack_win = &(container->stack_win);
                stack_win->window = create_window(conn, rect, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_CURSOR_LEFT_PTR, mask, values);

                /* Generate a graphics context for the titlebar */
                stack_win->gc = xcb_generate_id(conn);
                xcb_create_gc(conn, stack_win->gc, stack_win->window, 0, 0);

                stack_win->container = container;

                SLIST_INSERT_HEAD(&stack_wins, stack_win, stack_windows);
        } else {
                if (container->mode == MODE_STACK) {
                        /* When going out of stacking mode, we need to close the window */
                        struct Stack_Window *stack_win = &(container->stack_win);

                        SLIST_REMOVE(&stack_wins, stack_win, Stack_Window, stack_windows);

                        xcb_free_gc(conn, stack_win->gc);
                        xcb_destroy_window(conn, stack_win->window);

                        stack_win->width = -1;
                        stack_win->height = -1;
                }
        }
        container->mode = mode;

        /* Force reconfiguration of each client */
        Client *client;

        CIRCLEQ_FOREACH(client, &(container->clients), clients)
                client->force_reconfigure = true;

        render_layout(conn);
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
        /* clients without a container (docks) cannot be focused */
        assert(client->container != NULL);

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

                xcb_configure_window(conn, client->frame, mask, values);

                /* Child’s coordinates are relative to the parent (=frame) */
                values[0] = 0;
                values[1] = 0;
                xcb_configure_window(conn, client->child, mask, values);

                /* Raise the window */
                values[0] = XCB_STACK_MODE_ABOVE;
                xcb_configure_window(conn, client->frame, XCB_CONFIG_WINDOW_STACK_MODE, values);

        } else {
                printf("leaving fullscreen mode\n");
                /* Because the coordinates of the window haven’t changed, it would not be
                   re-configured if we don’t set the following flag */
                client->force_reconfigure = true;
                /* We left fullscreen mode, redraw the container */
                render_container(conn, client->container);
        }

        xcb_flush(conn);
}
