/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <xcb/xcb.h>

#include <xcb/xcb_wm.h>
#include <X11/XKBlib.h>

#include "i3.h"
#include "debug.h"
#include "table.h"
#include "layout.h"
#include "commands.h"
#include "data.h"
#include "xcb.h"
#include "util.h"
#include "xinerama.h"
#include "config.h"

/* After mapping/unmapping windows, a notify event is generated. However, we don’t want it,
   since it’d trigger an infinite loop of switching between the different windows when
   changing workspaces */
int ignore_notify_event = -1;

/*
 * Due to bindings like Mode_switch + <a>, we need to bind some keys in XCB_GRAB_MODE_SYNC.
 * Therefore, we just replay all key presses.
 *
 */
int handle_key_release(void *ignored, xcb_connection_t *conn, xcb_key_release_event_t *event) {
        printf("got key release, just passing\n");
        xcb_allow_events(conn, XCB_ALLOW_REPLAY_KEYBOARD, event->time);
        xcb_flush(conn);
        return 1;
}

/*
 * There was a key press. We compare this key code with our bindings table and pass
 * the bound action to parse_command().
 *
 */
int handle_key_press(void *ignored, xcb_connection_t *conn, xcb_key_press_event_t *event) {
        printf("Keypress %d\n", event->detail);

        /* We need to get the keysym group (There are group 1 to group 4, each holding
           two keysyms (without shift and with shift) using Xkb because X fails to
           provide them reliably (it works in Xephyr, it does not in real X) */
        XkbStateRec state;
        if (XkbGetState(xkbdpy, XkbUseCoreKbd, &state) == Success && (state.group+1) == 2)
                event->state |= 0x2;

        printf("state %d\n", event->state);

        /* Remove the numlock bit, all other bits are modifiers we can bind to */
        uint16_t state_filtered = event->state & ~XCB_MOD_MASK_LOCK;

        /* Find the binding */
        Binding *bind;
        TAILQ_FOREACH(bind, &bindings, bindings)
                if (bind->keycode == event->detail && bind->mods == state_filtered)
                        break;

        /* No match? Then it was an actively grabbed key, that is with Mode_switch, and
           the user did not press Mode_switch, so just pass it… */
        if (bind == TAILQ_END(&bindings)) {
                xcb_allow_events(conn, ReplayKeyboard, event->time);
                xcb_flush(conn);
                return 1;
        }

        parse_command(conn, bind->command);
        if (event->state & 0x2) {
                printf("Mode_switch -> allow_events(SyncKeyboard)\n");
                xcb_allow_events(conn, SyncKeyboard, event->time);
                xcb_flush(conn);
        }
        return 1;
}


/*
 * When the user moves the mouse pointer onto a window, this callback gets called.
 *
 */
int handle_enter_notify(void *ignored, xcb_connection_t *conn, xcb_enter_notify_event_t *event) {
        printf("enter_notify for %08x, serial %d\n", event->event, event->sequence);
        /* Some events are not interesting, because they were not generated actively by the
           user, but be reconfiguration of windows */
        if (event->sequence == ignore_notify_event) {
                printf("Ignoring, because of previous map\n");
                return 1;
        }

        /* This was either a focus for a client’s parent (= titlebar)… */
        Client *client = table_get(byParent, event->event);
        /* …or the client itself */
        if (client == NULL)
                client = table_get(byChild, event->event);

        /* If not, then the user moved his cursor to the root window. In that case, we adjust c_ws */
        if (client == NULL) {
                printf("Getting screen at %d x %d\n", event->root_x, event->root_y);
                i3Screen *screen = get_screen_containing(event->root_x, event->root_y);
                if (screen == NULL) {
                        printf("ERROR: No such screen\n");
                        return 0;
                }
                c_ws = &workspaces[screen->current_workspace];
                printf("We're now on virtual screen number %d\n", screen->num);
                return 1;
        }

        set_focus(conn, client);

        return 1;
}

int handle_button_press(void *ignored, xcb_connection_t *conn, xcb_button_press_event_t *event) {
        printf("button press!\n");
        /* This was either a focus for a client’s parent (= titlebar)… */
        Client *client = table_get(byChild, event->event);
        bool border_click = false;
        if (client == NULL) {
                client = table_get(byParent, event->event);
                border_click = true;
        }
        if (client == NULL) {
                /* The client was neither on a client’s titlebar nor on a client itself, maybe on a stack_window? */
                struct Stack_Window *stack_win;
                SLIST_FOREACH(stack_win, &stack_wins, stack_windows)
                        if (stack_win->window == event->event) {
                                /* A stack window was clicked. We calculate the destination client by
                                   dividing the Y position of the event through the height of a window
                                   decoration and then set the focus to this client. */
                                i3Font *font = load_font(conn, config.font);
                                int decoration_height = (font->height + 2 + 2);
                                int destination = (event->event_y / decoration_height),
                                    c = 0;
                                Client *client;

                                printf("Click on stack_win for client %d\n", destination);
                                CIRCLEQ_FOREACH(client, &(stack_win->container->clients), clients)
                                        if (c++ == destination) {
                                                set_focus(conn, client);
                                                return 1;
                                        }

                                return 1;
                        }

                return 1;
        }

        xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;
        xcb_screen_t *root_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

        /* Set focus in any case */
        set_focus(conn, client);

        /* Let’s see if this was on the borders (= resize). If not, we’re done */
        printf("press button on x=%d, y=%d\n", event->event_x, event->event_y);

        Container *con = client->container,
                  *first = NULL,
                  *second = NULL;
        enum { O_HORIZONTAL, O_VERTICAL } orientation = O_VERTICAL;

        if (con == NULL) {
                printf("dock. done.\n");
                xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
                xcb_flush(conn);
                return 1;
        }

        printf("event->event_x = %d, client->rect.width = %d\n", event->event_x, client->rect.width);

        if (!border_click) {
                printf("client. done.\n");
                xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
                xcb_flush(conn);
                return 1;
        }

        if (event->event_y < 2) {
                /* This was a press on the top border */
                if (con->row == 0)
                        return 1;
                first = con->workspace->table[con->col][con->row-1];
                second = con;
                orientation = O_HORIZONTAL;
        } else if (event->event_y >= (client->rect.height - 2)) {
                /* …bottom border */
                if (con->row == (con->workspace->rows-1))
                        return 1;
                first = con;
                second = con->workspace->table[con->col][con->row+1];
                orientation = O_HORIZONTAL;
        } else if (event->event_x <= 2) {
                /* …left border */
                if (con->col == 0)
                        return 1;
                first = con->workspace->table[con->col-1][con->row];
                second = con;
        } else if (event->event_x > 2) {
                /* …right border */
                if (con->col == (con->workspace->cols-1))
                        return 1;
                first = con;
                second = con->workspace->table[con->col+1][con->row];
        }

        /* Open a new window, the resizebar. Grab the pointer and move the window around
           as the user moves the pointer. */
        Rect grabrect = {0, 0, root_screen->width_in_pixels, root_screen->height_in_pixels};
        xcb_window_t grabwin = create_window(conn, grabrect, XCB_WINDOW_CLASS_INPUT_ONLY, -1, 0, NULL);

        Rect helprect;
        if (orientation == O_VERTICAL) {
                helprect.x = event->root_x;
                helprect.y = 0;
                helprect.width = 2;
                helprect.height = root_screen->height_in_pixels; /* this has to be the cell’s height */
        } else {
                helprect.x = 0;
                helprect.y = event->root_y;
                helprect.width = root_screen->width_in_pixels; /* this has to be the cell’s width */
                helprect.height = 2;
        }
        xcb_window_t helpwin = create_window(conn, helprect, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                             (orientation == O_VERTICAL ?
                                              XCB_CURSOR_SB_V_DOUBLE_ARROW :
                                              XCB_CURSOR_SB_H_DOUBLE_ARROW), 0, NULL);

        uint32_t values[1] = {get_colorpixel(conn, "#4c7899")};
        xcb_void_cookie_t cookie = xcb_change_window_attributes_checked(conn, helpwin, XCB_CW_BACK_PIXEL, values);
        check_error(conn, cookie, "Could not change window attributes (background color)");

        xcb_circulate_window(conn, XCB_CIRCULATE_RAISE_LOWEST, helpwin);

        xcb_grab_pointer(conn, false, root, XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION,
                        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, grabwin, XCB_NONE, XCB_CURRENT_TIME);

        xcb_flush(conn);

        xcb_generic_event_t *inside_event;
        /* I’ve always wanted to have my own eventhandler… */
        while ((inside_event = xcb_wait_for_event(conn))) {
                /* Same as get_event_handler in xcb */
                int nr = inside_event->response_type;
                if (nr == 0) {
                        /* An error occured */
                        handle_event(NULL, conn, inside_event);
                        free(inside_event);
                        continue;
                }
                assert(nr < 256);
                nr &= XCB_EVENT_RESPONSE_TYPE_MASK;
                assert(nr >= 2);

                /* Check if we need to escape this loop */
                if (nr == XCB_BUTTON_RELEASE)
                        break;

                switch (nr) {
                        case XCB_MOTION_NOTIFY:
                                if (orientation == O_VERTICAL) {
                                        values[0] = ((xcb_motion_notify_event_t*)inside_event)->root_x;
                                        xcb_configure_window(conn, helpwin, XCB_CONFIG_WINDOW_X, values);
                                } else {
                                        values[0] = ((xcb_motion_notify_event_t*)inside_event)->root_y;
                                        xcb_configure_window(conn, helpwin, XCB_CONFIG_WINDOW_Y, values);
                                }

                                xcb_flush(conn);
                                break;
                        case XCB_EXPOSE:
                                /* Use original handler */
                                xcb_event_handle(&evenths, inside_event);
                                break;
                        default:
                                printf("Ignoring event of type %d\n", nr);
                                break;
                }
                free(inside_event);
        }

        xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
        xcb_destroy_window(conn, helpwin);
        xcb_destroy_window(conn, grabwin);
        xcb_flush(conn);

        Workspace *ws = con->workspace;
        if (orientation == O_VERTICAL) {
                printf("Resize was from X = %d to X = %d\n", event->root_x, values[0]);

                /* Convert 0 (for default width_factor) to actual numbers */
                if (first->width_factor == 0)
                        first->width_factor = ((float)ws->rect.width / ws->cols) / ws->rect.width;
                if (second->width_factor == 0)
                        second->width_factor = ((float)ws->rect.width / ws->cols) / ws->rect.width;

                first->width_factor *= (float)(first->width + (values[0] - event->root_x)) / first->width;
                second->width_factor *= (float)(second->width - (values[0] - event->root_x)) / second->width;
        } else {
                printf("Resize was from Y = %d to Y = %d\n", event->root_y, values[0]);

                /* Convert 0 (for default height_factor) to actual numbers */
                if (first->height_factor == 0)
                        first->height_factor = ((float)ws->rect.height / ws->rows) / ws->rect.height;
                if (second->height_factor == 0)
                        second->height_factor = ((float)ws->rect.height / ws->rows) / ws->rect.height;

                first->height_factor *= (float)(first->height + (values[0] - event->root_y)) / first->height;
                second->height_factor *= (float)(second->height - (values[0] - event->root_y)) / second->height;
        }

        render_layout(conn);

        return 1;
}

/*
 * A new window appeared on the screen (=was mapped), so let’s manage it.
 *
 */
int handle_map_notify_event(void *prophs, xcb_connection_t *conn, xcb_map_notify_event_t *event) {
        window_attributes_t wa = { TAG_VALUE };
        wa.u.override_redirect = event->override_redirect;
        printf("MapNotify for 0x%08x, serial is %d.\n", event->window, event->sequence);
        printf("setting ignore_notify_event = %d\n", event->sequence);
        ignore_notify_event = event->sequence;
        manage_window(prophs, conn, event->window, wa);
        return 1;
}

/*
 * Configuration notifies are only handled because we need to set up ignore for the following
 * enter notify events
 *
 */
int handle_configure_event(void *prophs, xcb_connection_t *conn, xcb_configure_notify_event_t *event) {
        xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;

        printf("handle_configure_event\n");
        printf("event->x = %d, ->y = %d, ->width = %d, ->height = %d\n", event->x, event->y, event->width, event->height);
        printf("sequence = %d\n", event->sequence);

        ignore_notify_event = event->sequence;

        if (event->event == root) {
                printf("reconfigure of the root window, need to xinerama\n");
                /* FIXME: Somehow, this is occuring too often. Therefore, we check for 0/0,
                   but is there a better way? */
                if (event->x == 0 && event->y == 0)
                        xinerama_requery_screens(conn);
                return 1;
        }

        Client *client = table_get(byChild, event->window);
        if (client == NULL) {
                printf("client not managed, ignoring\n");
                return 1;
        }

        if (client->fullscreen) {
                printf("client in fullscreen, not touching\n");
                return 1;
        }

        /* Let’s see if the application has changed size/position on its own *sigh*… */
        if ((event->x != client->child_rect.x) ||
            (event->y != client->child_rect.y) ||
            (event->width != client->child_rect.width) ||
            (event->height != client->child_rect.height)) {
                /* Who is your window manager? Who’s that, huh? I AM YOUR WINDOW MANAGER! */
                printf("Application wanted to resize itself. Fixed that.\n");
                client->force_reconfigure = true;
                render_container(conn, client->container);
                xcb_flush(conn);
        }

        return 1;
}

/*
 * Our window decorations were unmapped. That means, the window will be killed now,
 * so we better clean up before.
 *
 */
int handle_unmap_notify_event(void *data, xcb_connection_t *conn, xcb_unmap_notify_event_t *event) {
        xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;

        printf("setting ignore_notify_event = %d\n", event->sequence);

        ignore_notify_event = event->sequence;

        Client *client = table_get(byChild, event->window);
        /* First, we need to check if the client is awaiting an unmap-request which
           was generated by us reparenting the window. In that case, we just ignore it. */
        if (client != NULL && client->awaiting_useless_unmap) {
                printf("Dropping this unmap request, it was generated by reparenting\n");
                client->awaiting_useless_unmap = false;
                return 1;
        }
        client = table_remove(byChild, event->window);

        printf("UnmapNotify for 0x%08x (received from 0x%08x): ", event->window, event->event);
        if (client == NULL) {
                printf("not a managed window. Ignoring.\n");
                return 0;
        }

        if (client->name != NULL)
                free(client->name);

        if (client->container != NULL) {
                Container *con = client->container;

                /* If this was the fullscreen client, we need to unset it */
                if (client->fullscreen)
                        con->workspace->fullscreen_client = NULL;

                /* If the container will be empty now and is in stacking mode, we need to
                   correctly resize the stack_win */
                if (CIRCLEQ_EMPTY(&(con->clients)) && con->mode == MODE_STACK) {
                        struct Stack_Window *stack_win = &(con->stack_win);
                        stack_win->rect.height = 0;
                        xcb_unmap_window(conn, stack_win->window);
                }

                /* Remove the client from the list of clients */
                CIRCLEQ_REMOVE(&(con->clients), client, clients);

                /* Remove from the focus stack */
                printf("Removing from focus stack\n");
                SLIST_REMOVE(&(con->workspace->focus_stack), client, Client, focus_clients);

                /* Remove from currently_focused */
                con->currently_focused = NULL;

                /* Actually set focus, if there is a window which should get it */
                if (!SLIST_EMPTY(&(con->workspace->focus_stack)))
                        set_focus(conn, SLIST_FIRST(&(con->workspace->focus_stack)));
        }

        printf("child of 0x%08x.\n", client->frame);
        xcb_reparent_window(conn, client->child, root, 0, 0);
        xcb_destroy_window(conn, client->frame);
        xcb_flush(conn);
        table_remove(byParent, client->frame);

        cleanup_table(conn, client->container->workspace);

        free(client);

        render_layout(conn);

        return 1;
}

/*
 * Called when a window changes its title
 *
 */
int handle_windowname_change(void *data, xcb_connection_t *conn, uint8_t state,
                                xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *prop) {
        printf("window's name changed.\n");
        Client *client = table_get(byChild, window);
        if (client == NULL)
                return 1;

        if (client->name != NULL)
                free(client->name);

        client->name_len = xcb_get_property_value_length(prop);
        client->name = smalloc(client->name_len);
        strncpy(client->name, xcb_get_property_value(prop), client->name_len);
        printf("rename to \"%.*s\".\n", client->name_len, client->name);

        if (client->container->mode == MODE_STACK)
                render_container(conn, client->container);
        else decorate_window(conn, client, client->frame, client->titlegc, 0);
        xcb_flush(conn);

        return 1;
}

/*
 * Expose event means we should redraw our windows (= title bar)
 *
 */
int handle_expose_event(void *data, xcb_connection_t *conn, xcb_expose_event_t *event) {
        printf("got expose_event\n");
        /* event->count is the number of minimum remaining expose events for this window, so we
           skip all events but the last one */
        if (event->count != 0)
                return 1;

        Client *client = table_get(byParent, event->window);
        if (client == NULL) {
                /* There was no client in the table, so this is probably an expose event for
                   one of our stack_windows. */
                struct Stack_Window *stack_win;
                SLIST_FOREACH(stack_win, &stack_wins, stack_windows)
                        if (stack_win->window == event->window) {
                                render_container(conn, stack_win->container);
                                return 1;
                        }
                return 1;
        }

        printf("handle_expose_event()\n");
        if (client->container->mode != MODE_STACK)
                decorate_window(conn, client, client->frame, client->titlegc, 0);
        else {
                xcb_change_gc_single(conn, client->titlegc, XCB_GC_FOREGROUND,
                        get_colorpixel(conn, "#285577"));

                xcb_rectangle_t rect = {0, 0, client->rect.width, client->rect.height};
                xcb_poly_fill_rectangle(conn, client->frame, client->titlegc, 1, &rect);

                xcb_flush(conn);
        }
        return 1;
}

/*
 * Handle client messages (EWMH)
 *
 */
int handle_client_message(void *data, xcb_connection_t *conn, xcb_client_message_event_t *event) {
        printf("client_message\n");

        if (event->type == atoms[_NET_WM_STATE]) {
                if (event->format != 32 || event->data.data32[1] != atoms[_NET_WM_STATE_FULLSCREEN])
                        return 0;

                printf("fullscreen\n");

                Client *client = table_get(byChild, event->window);
                if (client == NULL)
                        return 0;

                /* Check if the fullscreen state should be toggled */
                if ((client->fullscreen &&
                     (event->data.data32[0] == _NET_WM_STATE_REMOVE ||
                      event->data.data32[0] == _NET_WM_STATE_TOGGLE)) ||
                    (!client->fullscreen &&
                     (event->data.data32[0] == _NET_WM_STATE_ADD ||
                      event->data.data32[0] == _NET_WM_STATE_TOGGLE)))
                        toggle_fullscreen(conn, client);
        } else {
                printf("unhandled clientmessage\n");
                return 0;
        }

        return 1;
}

int window_type_handler(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                        xcb_atom_t atom, xcb_get_property_reply_t *property) {
        /* TODO: Implement this one. To do this, implement a little test program which sleep(1)s
         before changing this property. */
        printf("_NET_WM_WINDOW_TYPE changed, this is not yet implemented.\n");
        return 0;
}
