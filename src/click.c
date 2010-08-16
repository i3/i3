/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * src/click.c: Contains the handlers for button press (mouse click) events
 *              because they are quite large.
 *
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>

#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_icccm.h>

#include <X11/XKBlib.h>

#include "i3.h"
#include "queue.h"
#include "table.h"
#include "config.h"
#include "util.h"
#include "xcb.h"
#include "client.h"
#include "workspace.h"
#include "commands.h"
#include "floating.h"
#include "resize.h"
#include "log.h"
#include "randr.h"

static struct Stack_Window *get_stack_window(xcb_window_t window_id) {
        struct Stack_Window *current;

        SLIST_FOREACH(current, &stack_wins, stack_windows) {
                if (current->window != window_id)
                        continue;

                return current;
        }

        return NULL;
}

/*
 * Checks if the button press was on a stack window, handles focus setting and returns true
 * if so, or false otherwise.
 *
 */
static bool button_press_stackwin(xcb_connection_t *conn, xcb_button_press_event_t *event) {
        struct Stack_Window *stack_win;

        /* If we find a corresponding stack window, we can handle the event */
        if ((stack_win = get_stack_window(event->event)) == NULL)
                return false;

        /* A stack window was clicked, we check if it was button4 or button5
           which are scroll up / scroll down. */
        if (event->detail == XCB_BUTTON_INDEX_4 || event->detail == XCB_BUTTON_INDEX_5) {
                direction_t direction = (event->detail == XCB_BUTTON_INDEX_4 ? D_UP : D_DOWN);
                focus_window_in_container(conn, CUR_CELL, direction);
                return true;
        }

        /* It was no scrolling, so we calculate the destination client by
           dividing the Y position of the event through the height of a window
           decoration and then set the focus to this client. */
        i3Font *font = load_font(conn, config.font);
        int decoration_height = (font->height + 2 + 2);
        int destination = (event->event_y / decoration_height),
            c = 0,
            num_clients = 0;
        Client *client;
        Container *container = stack_win->container;

        CIRCLEQ_FOREACH(client, &(container->clients), clients)
                num_clients++;

        /* If we don’t have any clients in this container, we cannot do
         * anything useful anyways. */
        if (num_clients == 0)
                return true;

        if (container->mode == MODE_TABBED)
                destination = (event->event_x / (container->width / num_clients));
        else if (container->mode == MODE_STACK &&
                 container->stack_limit != STACK_LIMIT_NONE) {
                if (container->stack_limit == STACK_LIMIT_COLS) {
                        int wrap = ceil((float)num_clients / container->stack_limit_value);
                        int clicked_column = (event->event_x / (stack_win->rect.width / container->stack_limit_value));
                        int clicked_row = (event->event_y / decoration_height);
                        DLOG("clicked on column %d, row %d\n", clicked_column, clicked_row);
                        destination = (wrap * clicked_column) + clicked_row;
                } else {
                        int width = (stack_win->rect.width / ceil((float)num_clients / container->stack_limit_value));
                        int clicked_column = (event->event_x / width);
                        int clicked_row = (event->event_y / decoration_height);
                        DLOG("clicked on column %d, row %d\n", clicked_column, clicked_row);
                        destination = (container->stack_limit_value * clicked_column) + clicked_row;
                }
        }

        DLOG("Click on stack_win for client %d\n", destination);
        CIRCLEQ_FOREACH(client, &(stack_win->container->clients), clients)
                if (c++ == destination) {
                        set_focus(conn, client, true);
                        return true;
                }

        return true;
}

/*
 * Checks if the button press was on a bar, switches to the workspace and returns true
 * if so, or false otherwise.
 *
 */
static bool button_press_bar(xcb_connection_t *conn, xcb_button_press_event_t *event) {
        Output *output;
        TAILQ_FOREACH(output, &outputs, outputs) {
                if (output->bar != event->event)
                        continue;

                DLOG("Click on a bar\n");

                /* Check if the button was one of button4 or button5 (scroll up / scroll down) */
                if (event->detail == XCB_BUTTON_INDEX_4 || event->detail == XCB_BUTTON_INDEX_5) {
                        Workspace *ws = c_ws;
                        if (event->detail == XCB_BUTTON_INDEX_5) {
                                while ((ws = TAILQ_NEXT(ws, workspaces)) != TAILQ_END(workspaces_head)) {
                                        if (ws->output == output) {
                                                workspace_show(conn, ws->num + 1);
                                                return true;
                                        }
                                }
                        } else {
                                while ((ws = TAILQ_PREV(ws, workspaces_head, workspaces)) != TAILQ_END(workspaces)) {
                                        if (ws->output == output) {
                                                workspace_show(conn, ws->num + 1);
                                                return true;
                                        }
                                }
                        }
                        return true;
                }
                int drawn = 0;
                /* Because workspaces can be on different outputs, we need to loop
                   through all of them and decide to count it based on its ->output */
                Workspace *ws;
                TAILQ_FOREACH(ws, workspaces, workspaces) {
                        if (ws->output != output)
                                continue;
                        DLOG("Checking if click was on workspace %d with drawn = %d, tw = %d\n",
                                        ws->num, drawn, ws->text_width);
                        if (event->event_x > (drawn + 1) &&
                            event->event_x <= (drawn + 1 + ws->text_width + 5 + 5)) {
                                workspace_show(conn, ws->num + 1);
                                return true;
                        }

                        drawn += ws->text_width + 5 + 5 + 2;
                }
                return true;
        }

        return false;
}

/*
 * Called when the user clicks using the floating_modifier, but the client is in
 * tiling layout.
 *
 * Returns false if it does not do anything (that is, the click should be sent
 * to the client).
 *
 */
static bool floating_mod_on_tiled_client(xcb_connection_t *conn, Client *client,
                                         xcb_button_press_event_t *event) {
        /* Only the right mouse button is interesting for us at the moment */
        if (event->detail != 3)
                return false;

        /* The client is in tiling layout. We can still
         * initiate a resize with the right mouse button,
         * by chosing the border which is the most near one
         * to the position of the mouse pointer */
        int to_right = client->rect.width - event->event_x,
            to_left = event->event_x,
            to_top = event->event_y,
            to_bottom = client->rect.height - event->event_y;
        resize_orientation_t orientation = O_VERTICAL;
        Container *con = client->container;
        Workspace *ws = con->workspace;
        int first = 0, second = 0;

        DLOG("click was %d px to the right, %d px to the left, %d px to top, %d px to bottom\n",
                        to_right, to_left, to_top, to_bottom);

        if (to_right < to_left &&
            to_right < to_top &&
            to_right < to_bottom) {
                /* …right border */
                first = con->col + (con->colspan - 1);
                DLOG("column %d\n", first);

                if (!cell_exists(ws, first, con->row) ||
                    (first == (ws->cols-1)))
                        return false;

                second = first + 1;
        } else if (to_left < to_right &&
                   to_left < to_top &&
                   to_left < to_bottom) {
                /* …left border */
                if (con->col == 0)
                        return false;

                first = con->col - 1;
                second = con->col;
        } else if (to_top < to_right &&
                   to_top < to_left &&
                   to_top < to_bottom) {
                /* This was a press on the top border */
                if (con->row == 0)
                        return false;
                first = con->row - 1;
                second = con->row;
                orientation = O_HORIZONTAL;
        } else if (to_bottom < to_right &&
                   to_bottom < to_left &&
                   to_bottom < to_top) {
                /* …bottom border */
                first = con->row + (con->rowspan - 1);
                if (!cell_exists(ws, con->col, first) ||
                    (first == (ws->rows-1)))
                        return false;

                second = first + 1;
                orientation = O_HORIZONTAL;
        }

       return resize_graphical_handler(conn, ws, first, second, orientation, event);
}

int handle_button_press(void *ignored, xcb_connection_t *conn, xcb_button_press_event_t *event) {
        DLOG("Button %d pressed\n", event->state);
        /* This was either a focus for a client’s parent (= titlebar)… */
        Client *client = table_get(&by_child, event->event);
        bool border_click = false;
        if (client == NULL) {
                client = table_get(&by_parent, event->event);
                border_click = true;
        }
        /* See if this was a click with the configured modifier. If so, we need
         * to move around the client if it was floating. if not, we just process
         * as usual. */
        if (config.floating_modifier != 0 &&
            (event->state & config.floating_modifier) == config.floating_modifier) {
                if (client == NULL) {
                        DLOG("Not handling, floating_modifier was pressed and no client found\n");
                        xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
                        xcb_flush(conn);
                        return 1;
                }
                if (client->fullscreen) {
                        DLOG("Not handling, client is in fullscreen mode\n");
                        xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
                        xcb_flush(conn);
                        return 1;
                }
                if (client_is_floating(client)) {
                        DLOG("button %d pressed\n", event->detail);
                        if (event->detail == 1) {
                                DLOG("left mouse button, dragging\n");
                                floating_drag_window(conn, client, event);
                        } else if (event->detail == 3) {
                                bool proportional = (event->state & BIND_SHIFT);
                                DLOG("right mouse button\n");
                                floating_resize_window(conn, client, proportional, event);
                        }
                        return 1;
                }

                if (!floating_mod_on_tiled_client(conn, client, event)) {
                        xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
                        xcb_flush(conn);
                }

                return 1;
        }

        if (client == NULL) {
                /* The client was neither on a client’s titlebar nor on a client itself, maybe on a stack_window? */
                if (button_press_stackwin(conn, event))
                        return 1;

                /* Or on a bar? */
                if (button_press_bar(conn, event))
                        return 1;

                DLOG("Could not handle this button press\n");
                xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
                xcb_flush(conn);
                return 1;
        }

        /* Set focus in any case */
        set_focus(conn, client, false);

        /* Let’s see if this was on the borders (= resize). If not, we’re done */
        DLOG("press button on x=%d, y=%d\n", event->event_x, event->event_y);
        resize_orientation_t orientation = O_VERTICAL;
        Container *con = client->container;
        int first, second;

        if (client->dock) {
                DLOG("dock. done.\n");
                xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
                xcb_flush(conn);
                return 1;
        }

        DLOG("event->event_x = %d, client->rect.width = %d\n", event->event_x, client->rect.width);

        /* Some clients (xfontsel for example) seem to pass clicks on their
         * window to the parent window, thus we receive an event here which in
         * reality is a border_click. Check for the position and fix state. */
        if (border_click &&
            event->event_x >= client->child_rect.x &&
            event->event_x <= (client->child_rect.x + client->child_rect.width) &&
            event->event_y >= client->child_rect.y &&
            event->event_y <= (client->child_rect.y + client->child_rect.height)) {
                DLOG("Fixing border_click = false because of click in child\n");
                border_click = false;
        }

        if (!border_click) {
                DLOG("client. done.\n");
                xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
                /* Floating clients should be raised on click */
                if (client_is_floating(client))
                        xcb_raise_window(conn, client->frame);
                xcb_flush(conn);
                return 1;
        }

        /* Don’t handle events inside the titlebar, only borders are interesting */
        i3Font *font = load_font(conn, config.font);
        if (event->event_y >= 2 && event->event_y <= (font->height + 2 + 2)) {
                DLOG("click on titlebar\n");

                /* Floating clients can be dragged by grabbing their titlebar */
                if (client_is_floating(client)) {
                        /* Firstly, we raise it. Maybe the user just wanted to raise it without grabbing */
                        xcb_raise_window(conn, client->frame);
                        xcb_flush(conn);

                        floating_drag_window(conn, client, event);
                }
                return 1;
        }

        if (client_is_floating(client))
                return floating_border_click(conn, client, event);

        Workspace *ws = con->workspace;

        if (event->event_y < 2) {
                /* This was a press on the top border */
                if (con->row == 0)
                        return 1;
                first = con->row - 1;
                second = con->row;
                orientation = O_HORIZONTAL;
        } else if (event->event_y >= (client->rect.height - 2)) {
                /* …bottom border */
                first = con->row + (con->rowspan - 1);
                if (!cell_exists(ws, con->col, first) ||
                    (first == (ws->rows-1)))
                        return 1;

                second = first + 1;
                orientation = O_HORIZONTAL;
        } else if (event->event_x <= 2) {
                /* …left border */
                if (con->col == 0)
                        return 1;

                first = con->col - 1;
                second = con->col;
        } else if (event->event_x > 2) {
                /* …right border */
                first = con->col + (con->colspan - 1);
                DLOG("column %d\n", first);

                if (!cell_exists(ws, first, con->row) ||
                    (first == (ws->cols-1)))
                        return 1;

                second = first + 1;
        }

        return resize_graphical_handler(conn, ws, first, second, orientation, event);
}
