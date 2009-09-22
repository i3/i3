/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
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

/*
 * Checks if the button press was on a stack window, handles focus setting and returns true
 * if so, or false otherwise.
 *
 */
static bool button_press_stackwin(xcb_connection_t *conn, xcb_button_press_event_t *event) {
        struct Stack_Window *stack_win;
        SLIST_FOREACH(stack_win, &stack_wins, stack_windows) {
                if (stack_win->window != event->event)
                        continue;

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

                CIRCLEQ_FOREACH(client, &(stack_win->container->clients), clients)
                        num_clients++;

                if (stack_win->container->mode == MODE_TABBED)
                        destination = (event->event_x / (stack_win->container->width / num_clients));

                LOG("Click on stack_win for client %d\n", destination);
                CIRCLEQ_FOREACH(client, &(stack_win->container->clients), clients)
                        if (c++ == destination) {
                                set_focus(conn, client, true);
                                return true;
                        }

                return true;
        }

        return false;
}

/*
 * Checks if the button press was on a bar, switches to the workspace and returns true
 * if so, or false otherwise.
 *
 */
static bool button_press_bar(xcb_connection_t *conn, xcb_button_press_event_t *event) {
        i3Screen *screen;
        TAILQ_FOREACH(screen, virtual_screens, screens) {
                if (screen->bar != event->event)
                        continue;

                LOG("Click on a bar\n");

                /* Check if the button was one of button4 or button5 (scroll up / scroll down) */
                if (event->detail == XCB_BUTTON_INDEX_4 || event->detail == XCB_BUTTON_INDEX_5) {
                        int add = (event->detail == XCB_BUTTON_INDEX_4 ? -1 : 1);
                        for (int i = c_ws->num + add; (i >= 0) && (i < 10); i += add)
                                if (workspaces[i].screen == screen) {
                                        workspace_show(conn, i+1);
                                        return true;
                                }
                        return true;
                }
                int drawn = 0;
                /* Because workspaces can be on different screens, we need to loop
                   through all of them and decide to count it based on its ->screen */
                for (int i = 0; i < 10; i++) {
                        if (workspaces[i].screen != screen)
                                continue;
                        LOG("Checking if click was on workspace %d with drawn = %d, tw = %d\n",
                                        i, drawn, workspaces[i].text_width);
                        if (event->event_x > (drawn + 1) &&
                            event->event_x <= (drawn + 1 + workspaces[i].text_width + 5 + 5)) {
                                workspace_show(conn, i+1);
                                return true;
                        }

                        drawn += workspaces[i].text_width + 5 + 5 + 2;
                }
                return true;
        }

        return false;
}

int handle_button_press(void *ignored, xcb_connection_t *conn, xcb_button_press_event_t *event) {
        LOG("Button %d pressed\n", event->state);
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
            (event->state & config.floating_modifier) != 0) {
                if (client == NULL) {
                        LOG("Not handling, floating_modifier was pressed and no client found\n");
                        return 1;
                }
                if (client_is_floating(client)) {
                        LOG("button %d pressed\n", event->detail);
                        if (event->detail == 1) {
                                LOG("left mouse button, dragging\n");
                                floating_drag_window(conn, client, event);
                        } else if (event->detail == 3) {
                                LOG("right mouse button\n");
                                floating_resize_window(conn, client, event);
                        }
                        return 1;
                } else {
                        /* The client is in tiling layout. We can still
                         * initiate a resize with the right mouse button,
                         * by chosing the border which is the most near one
                         * to the position of the mouse pointer */
                        if (event->detail == 3) {
                                int to_right = client->rect.width - event->event_x,
                                    to_left = event->event_x,
                                    to_top = event->event_y,
                                    to_bottom = client->rect.height - event->event_y;
                                resize_orientation_t orientation = O_VERTICAL;
                                Container *con = client->container;
                                int first = 0, second = 0;

                                LOG("click was %d px to the right, %d px to the left, %d px to top, %d px to bottom\n",
                                                to_right, to_left, to_top, to_bottom);

                                if (to_right < to_left &&
                                    to_right < to_top &&
                                    to_right < to_bottom) {
                                        /* …right border */
                                        first = con->col + (con->colspan - 1);
                                        LOG("column %d\n", first);

                                        if (!cell_exists(first, con->row) ||
                                            (first == (con->workspace->cols-1)))
                                                return 1;

                                        second = first + 1;
                                } else if (to_left < to_right &&
                                           to_left < to_top &&
                                           to_left < to_bottom) {
                                        /* …left border */
                                        if (con->col == 0)
                                                return 1;

                                        first = con->col - 1;
                                        second = con->col;
                                } else if (to_top < to_right &&
                                           to_top < to_left &&
                                           to_top < to_bottom) {
                                        /* This was a press on the top border */
                                        if (con->row == 0)
                                                return 1;
                                        first = con->row - 1;
                                        second = con->row;
                                        orientation = O_HORIZONTAL;
                                } else if (to_bottom < to_right &&
                                           to_bottom < to_left &&
                                           to_bottom < to_top) {
                                        /* …bottom border */
                                        first = con->row + (con->rowspan - 1);
                                        if (!cell_exists(con->col, first) ||
                                            (first == (con->workspace->rows-1)))
                                                return 1;

                                        second = first + 1;
                                        orientation = O_HORIZONTAL;
                                }

                               return resize_graphical_handler(conn, con->workspace, first, second, orientation, event);
                        }
                }
        }

        if (client == NULL) {
                /* The client was neither on a client’s titlebar nor on a client itself, maybe on a stack_window? */
                if (button_press_stackwin(conn, event))
                        return 1;

                /* Or on a bar? */
                if (button_press_bar(conn, event))
                        return 1;

                LOG("Could not handle this button press\n");
                return 1;
        }

        /* Set focus in any case */
        set_focus(conn, client, true);

        /* Let’s see if this was on the borders (= resize). If not, we’re done */
        LOG("press button on x=%d, y=%d\n", event->event_x, event->event_y);
        resize_orientation_t orientation = O_VERTICAL;
        Container *con = client->container;
        int first, second;

        if (client->dock) {
                LOG("dock. done.\n");
                xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
                xcb_flush(conn);
                return 1;
        }

        LOG("event->event_x = %d, client->rect.width = %d\n", event->event_x, client->rect.width);

        /* Some clients (xfontsel for example) seem to pass clicks on their
         * window to the parent window, thus we receive an event here which in
         * reality is a border_click. Check for the position and fix state. */
        if (border_click &&
            event->event_x >= client->child_rect.x &&
            event->event_x <= (client->child_rect.x + client->child_rect.width) &&
            event->event_y >= client->child_rect.y &&
            event->event_y <= (client->child_rect.y + client->child_rect.height)) {
                LOG("Fixing border_click = false because of click in child\n");
                border_click = false;
        }

        if (!border_click) {
                LOG("client. done.\n");
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
                LOG("click on titlebar\n");

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
                if (!cell_exists(con->col, first) ||
                    (first == (con->workspace->rows-1)))
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
                LOG("column %d\n", first);

                if (!cell_exists(first, con->row) ||
                    (first == (con->workspace->cols-1)))
                        return 1;

                second = first + 1;
        }

        return resize_graphical_handler(conn, con->workspace, first, second, orientation, event);
}
