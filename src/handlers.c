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
#include <time.h>

#include <xcb/xcb.h>
#include <xcb/xcb_wm.h>
#include <xcb/xcb_icccm.h>

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
#include "queue.h"

/* After mapping/unmapping windows, a notify event is generated. However, we don’t want it,
   since it’d trigger an infinite loop of switching between the different windows when
   changing workspaces */
static SLIST_HEAD(ignore_head, Ignore_Event) ignore_events;

static void add_ignore_event(const int sequence) {
        struct Ignore_Event *event = smalloc(sizeof(struct Ignore_Event));

        event->sequence = sequence;
        event->added = time(NULL);

        LOG("Adding sequence %d to ignorelist\n", sequence);

        SLIST_INSERT_HEAD(&ignore_events, event, ignore_events);
}

/*
 * Checks if the given sequence is ignored and returns true if so.
 *
 */
static bool event_is_ignored(const int sequence) {
        struct Ignore_Event *event;
        time_t now = time(NULL);
        for (event = SLIST_FIRST(&ignore_events); event != SLIST_END(&ignore_events);) {
                if ((now - event->added) > 5) {
                        LOG("Entry is older than five seconds, cleaning up\n");
                        struct Ignore_Event *save = event;
                        event = SLIST_NEXT(event, ignore_events);
                        SLIST_REMOVE(&ignore_events, save, Ignore_Event, ignore_events);
                        free(save);
                } else event = SLIST_NEXT(event, ignore_events);
        }

        SLIST_FOREACH(event, &ignore_events, ignore_events) {
                if (event->sequence == sequence) {
                        LOG("Ignoring event (sequence %d)\n", sequence);
                        SLIST_REMOVE(&ignore_events, event, Ignore_Event, ignore_events);
                        free(event);
                        return true;
                }
        }

        return false;
}

/*
 * Due to bindings like Mode_switch + <a>, we need to bind some keys in XCB_GRAB_MODE_SYNC.
 * Therefore, we just replay all key presses.
 *
 */
int handle_key_release(void *ignored, xcb_connection_t *conn, xcb_key_release_event_t *event) {
        LOG("got key release, just passing\n");
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
        LOG("Keypress %d\n", event->detail);

        /* We need to get the keysym group (There are group 1 to group 4, each holding
           two keysyms (without shift and with shift) using Xkb because X fails to
           provide them reliably (it works in Xephyr, it does not in real X) */
        XkbStateRec state;
        if (XkbGetState(xkbdpy, XkbUseCoreKbd, &state) == Success && (state.group+1) == 2)
                event->state |= 0x2;

        LOG("state %d\n", event->state);

        /* Remove the numlock bit, all other bits are modifiers we can bind to */
        uint16_t state_filtered = event->state & ~(xcb_numlock_mask | XCB_MOD_MASK_LOCK);

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
                LOG("Mode_switch -> allow_events(SyncKeyboard)\n");
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
        LOG("enter_notify for %08x, mode = %d, detail %d, serial %d\n", event->event, event->mode, event->detail, event->sequence);
        if (event->mode != XCB_NOTIFY_MODE_NORMAL) {
                LOG("This was not a normal notify, ignoring\n");
                return 1;
        }
        /* Some events are not interesting, because they were not generated actively by the
           user, but be reconfiguration of windows */
        if (event_is_ignored(event->sequence))
                return 1;

        /* This was either a focus for a client’s parent (= titlebar)… */
        Client *client = table_get(byParent, event->event);
        /* …or the client itself */
        if (client == NULL)
                client = table_get(byChild, event->event);

        /* Check for stack windows */
        if (client == NULL) {
                struct Stack_Window *stack_win;
                SLIST_FOREACH(stack_win, &stack_wins, stack_windows)
                        if (stack_win->window == event->event) {
                                client = stack_win->container->currently_focused;
                                break;
                        }
        }


        /* If not, then the user moved his cursor to the root window. In that case, we adjust c_ws */
        if (client == NULL) {
                LOG("Getting screen at %d x %d\n", event->root_x, event->root_y);
                i3Screen *screen = get_screen_containing(event->root_x, event->root_y);
                if (screen == NULL) {
                        LOG("ERROR: No such screen\n");
                        return 0;
                }
                c_ws->current_row = current_row;
                c_ws->current_col = current_col;
                c_ws = &workspaces[screen->current_workspace];
                current_row = c_ws->current_row;
                current_col = c_ws->current_col;
                LOG("We're now on virtual screen number %d\n", screen->num);
                return 1;
        }

        /* Do plausibility checks: This event may be useless for us if it occurs on a window
           which is in a stacked container but not the focused one */
        if (client->container != NULL &&
            client->container->mode == MODE_STACK &&
            client->container->currently_focused != client) {
                LOG("Plausibility check says: no\n");
                return 1;
        }

        set_focus(conn, client, false);

        return 1;
}

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
                    c = 0;
                Client *client;

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
                                        show_workspace(conn, i+1);
                                        return true;
                                }
                        return true;
                }
                i3Font *font = load_font(conn, config.font);
                int workspace = event->event_x / (font->height + 6),
                    c = 0;
                /* Because workspaces can be on different screens, we need to loop
                   through all of them and decide to count it based on its ->screen */
                for (int i = 0; i < 10; i++)
                        if ((workspaces[i].screen == screen) && (c++ == workspace)) {
                                show_workspace(conn, i+1);
                                return true;
                        }
                return true;
        }

        return false;
}

int handle_button_press(void *ignored, xcb_connection_t *conn, xcb_button_press_event_t *event) {
        LOG("button press!\n");
        /* This was either a focus for a client’s parent (= titlebar)… */
        Client *client = table_get(byChild, event->event);
        bool border_click = false;
        if (client == NULL) {
                client = table_get(byParent, event->event);
                border_click = true;
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

        xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;
        xcb_screen_t *root_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

        /* Set focus in any case */
        set_focus(conn, client, true);

        /* Let’s see if this was on the borders (= resize). If not, we’re done */
        LOG("press button on x=%d, y=%d\n", event->event_x, event->event_y);

        Container *con = client->container,
                  *first = NULL,
                  *second = NULL;
        enum { O_HORIZONTAL, O_VERTICAL } orientation = O_VERTICAL;
        int new_position;

        if (con == NULL) {
                LOG("dock. done.\n");
                xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
                xcb_flush(conn);
                return 1;
        }

        LOG("event->event_x = %d, client->rect.width = %d\n", event->event_x, client->rect.width);

        if (!border_click) {
                LOG("client. done.\n");
                xcb_allow_events(conn, XCB_ALLOW_REPLAY_POINTER, event->time);
                xcb_flush(conn);
                return 1;
        }

        /* Don’t handle events inside the titlebar, only borders are interesting */
        i3Font *font = load_font(conn, config.font);
        if (event->event_y >= 2 && event->event_y <= (font->height + 2 + 2)) {
                LOG("click on titlebar\n");
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

        /* FIXME: horizontal resizing causes empty spaces to exist */
        if (orientation == O_HORIZONTAL) {
                LOG("Sorry, horizontal resizing is not yet activated due to creating layout bugs."
                    "If you are brave, enable the code for yourself and try fixing it.\n");
                return 1;
        }

        uint32_t mask = 0;
        uint32_t values[2];

        mask = XCB_CW_OVERRIDE_REDIRECT;
        values[0] = 1;

        /* Open a new window, the resizebar. Grab the pointer and move the window around
           as the user moves the pointer. */
        Rect grabrect = {0, 0, root_screen->width_in_pixels, root_screen->height_in_pixels};
        xcb_window_t grabwin = create_window(conn, grabrect, XCB_WINDOW_CLASS_INPUT_ONLY, -1, mask, values);

        Rect helprect;
        if (orientation == O_VERTICAL) {
                helprect.x = event->root_x;
                helprect.y = 0;
                helprect.width = 2;
                helprect.height = root_screen->height_in_pixels; /* this has to be the cell’s height */
                new_position = event->root_x;
        } else {
                helprect.x = 0;
                helprect.y = event->root_y;
                helprect.width = root_screen->width_in_pixels; /* this has to be the cell’s width */
                helprect.height = 2;
                new_position = event->root_y;
        }

        mask = XCB_CW_BACK_PIXEL;
        values[0] = get_colorpixel(conn, "#4c7899");

        mask |= XCB_CW_OVERRIDE_REDIRECT;
        values[1] = 1;

        xcb_window_t helpwin = create_window(conn, helprect, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                             (orientation == O_VERTICAL ?
                                              XCB_CURSOR_SB_V_DOUBLE_ARROW :
                                              XCB_CURSOR_SB_H_DOUBLE_ARROW), mask, values);

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
                                        values[0] = new_position = ((xcb_motion_notify_event_t*)inside_event)->root_x;
                                        xcb_configure_window(conn, helpwin, XCB_CONFIG_WINDOW_X, values);
                                } else {
                                        values[0] = new_position = ((xcb_motion_notify_event_t*)inside_event)->root_y;
                                        xcb_configure_window(conn, helpwin, XCB_CONFIG_WINDOW_Y, values);
                                }

                                xcb_flush(conn);
                                break;
                        default:
                                LOG("Passing to original handler\n");
                                /* Use original handler */
                                xcb_event_handle(&evenths, inside_event);
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
                LOG("Resize was from X = %d to X = %d\n", event->root_x, new_position);
                if (event->root_x == new_position) {
                        LOG("Nothing changed, not updating anything\n");
                        return 1;
                }

                /* Convert 0 (for default width_factor) to actual numbers */
                if (first->width_factor == 0)
                        first->width_factor = ((float)ws->rect.width / ws->cols) / ws->rect.width;
                if (second->width_factor == 0)
                        second->width_factor = ((float)ws->rect.width / ws->cols) / ws->rect.width;

                first->width_factor *= (float)(first->width + (new_position - event->root_x)) / first->width;
                second->width_factor *= (float)(second->width - (new_position - event->root_x)) / second->width;
        } else {
                LOG("Resize was from Y = %d to Y = %d\n", event->root_y, new_position);
                if (event->root_y == new_position) {
                        LOG("Nothing changed, not updating anything\n");
                        return 1;
                }

                /* Convert 0 (for default height_factor) to actual numbers */
                if (first->height_factor == 0)
                        first->height_factor = ((float)ws->rect.height / ws->rows) / ws->rect.height;
                if (second->height_factor == 0)
                        second->height_factor = ((float)ws->rect.height / ws->rows) / ws->rect.height;

                first->height_factor *= (float)(first->height + (new_position - event->root_y)) / first->height;
                second->height_factor *= (float)(second->height - (new_position - event->root_y)) / second->height;
        }

        render_layout(conn);

        return 1;
}

/*
 * A new window appeared on the screen (=was mapped), so let’s manage it.
 *
 */
int handle_map_request(void *prophs, xcb_connection_t *conn, xcb_map_request_event_t *event) {
        xcb_get_window_attributes_cookie_t cookie;
        xcb_get_window_attributes_reply_t *reply;

        cookie = xcb_get_window_attributes_unchecked(conn, event->window);

        if ((reply = xcb_get_window_attributes_reply(conn, cookie, NULL)) == NULL) {
                LOG("Could not get window attributes\n");
                return -1;
        }

        window_attributes_t wa = { TAG_VALUE };
        LOG("override_redirect = %d\n", reply->override_redirect);
        wa.u.override_redirect = reply->override_redirect;
        LOG("window = 0x%08x, serial is %d.\n", event->window, event->sequence);
        add_ignore_event(event->sequence);

        manage_window(prophs, conn, event->window, wa);
        return 1;
}

/*
 * Configure requests are received when the application wants to resize windows on their own.
 *
 * We generate a synthethic configure notify event to signalize the client its "new" position.
 *
 */
int handle_configure_request(void *prophs, xcb_connection_t *conn, xcb_configure_request_event_t *event) {
        LOG("configure-request, serial %d\n", event->sequence);
        LOG("event->window = %08x\n", event->window);
        LOG("application wants to be at %dx%d with %dx%d\n", event->x, event->y, event->width, event->height);

        Client *client = table_get(byChild, event->window);
        if (client == NULL) {
                LOG("This client is not mapped, so we don't care and just tell the client that he will get its size\n");
                Rect rect = {event->x, event->y, event->width, event->height};
                fake_configure_notify(conn, rect, event->window);
                return 1;
        }

        fake_absolute_configure_notify(conn, client);

        return 1;
}

/*
 * Configuration notifies are only handled because we need to set up ignore for the following
 * enter notify events
 *
 */
int handle_configure_event(void *prophs, xcb_connection_t *conn, xcb_configure_notify_event_t *event) {
        xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;

        LOG("handle_configure_event for window %08x\n", event->window);
        LOG("event->type = %d, \n", event->response_type);
        LOG("event->x = %d, ->y = %d, ->width = %d, ->height = %d\n", event->x, event->y, event->width, event->height);
        add_ignore_event(event->sequence);

        if (event->event == root) {
                LOG("reconfigure of the root window, need to xinerama\n");
                /* FIXME: Somehow, this is occuring too often. Therefore, we check for 0/0,
                   but is there a better way? */
                if (event->x == 0 && event->y == 0)
                        xinerama_requery_screens(conn);
                return 1;
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

        add_ignore_event(event->sequence);

        Client *client = table_get(byChild, event->window);
        /* First, we need to check if the client is awaiting an unmap-request which
           was generated by us reparenting the window. In that case, we just ignore it. */
        if (client != NULL && client->awaiting_useless_unmap) {
                LOG("Dropping this unmap request, it was generated by reparenting\n");
                client->awaiting_useless_unmap = false;
                return 1;
        }

        LOG("event->window = %08x, event->event = %08x\n", event->window, event->event);
        LOG("UnmapNotify for 0x%08x (received from 0x%08x)\n", event->window, event->event);
        if (client == NULL) {
                LOG("not a managed window. Ignoring.\n");
                return 0;
        }

        client = table_remove(byChild, event->window);

        if (client->name != NULL)
                free(client->name);

        if (client->container != NULL) {
                Container *con = client->container;

                /* If this was the fullscreen client, we need to unset it */
                if (client->fullscreen)
                        con->workspace->fullscreen_client = NULL;

                /* Remove the client from the list of clients */
                remove_client_from_container(conn, client, con);

                /* Set focus to the last focused client in this container */
                con->currently_focused = get_last_focused_client(conn, con, NULL);

                /* Only if this is the active container, we need to really change focus */
                if ((con->currently_focused != NULL) && (con == CUR_CELL))
                        set_focus(conn, con->currently_focused, false);
        }

        if (client->dock) {
                LOG("Removing from dock clients\n");
                SLIST_REMOVE(&(client->workspace->screen->dock_clients), client, Client, dock_clients);
        }

        LOG("child of 0x%08x.\n", client->frame);
        xcb_reparent_window(conn, client->child, root, 0, 0);
        xcb_destroy_window(conn, client->frame);
        xcb_flush(conn);
        table_remove(byParent, client->frame);

        if (client->container != NULL) {
                cleanup_table(conn, client->container->workspace);
                fix_colrowspan(conn, client->container->workspace);
        }

        /* Let’s see how many clients there are left on the workspace to delete it if it’s empty */
        bool workspace_empty = true;
        FOR_TABLE(client->workspace)
                if (!CIRCLEQ_EMPTY(&(client->workspace->table[cols][rows]->clients))) {
                        workspace_empty = false;
                        break;
                }

        i3Screen *screen;
        TAILQ_FOREACH(screen, virtual_screens, screens)
                if (screen->current_workspace == client->workspace->num) {
                        workspace_empty = false;
                        break;
                }

        if (workspace_empty)
                client->workspace->screen = NULL;

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
        LOG("window's name changed.\n");
        if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
                LOG("_NET_WM_NAME not specified, not changing\n");
                return 1;
        }
        Client *client = table_get(byChild, window);
        if (client == NULL)
                return 1;

        /* Save the old pointer to make the update atomic */
        char *new_name;
        int new_len;
        asprintf(&new_name, "%.*s", xcb_get_property_value_length(prop), (char*)xcb_get_property_value(prop));
        /* Convert it to UCS-2 here for not having to convert it later every time we want to pass it to X */
        char *ucs2_name = convert_utf8_to_ucs2(new_name, &new_len);
        LOG("Name should change to \"%s\"\n", new_name);
        free(new_name);

        /* Check if they are the same and don’t update if so.
           Note the use of new_len * 2 to check all bytes as each glyph takes 2 bytes.
           Also note the use of memcmp() instead of strncmp() because the latter stops on nullbytes,
           but UCS-2 uses nullbytes to fill up glyphs which only use one byte. */
        if ((new_len == client->name_len) &&
            (client->name != NULL) &&
            (memcmp(client->name, ucs2_name, new_len * 2) == 0)) {
                LOG("Name did not change, not updating\n");
                free(ucs2_name);
                return 1;
        }

        char *old_name = client->name;
        client->name = ucs2_name;
        client->name_len = new_len;
        client->uses_net_wm_name = true;

        if (old_name != NULL)
                free(old_name);

        /* If the client is a dock window, we don’t need to render anything */
        if (client->dock)
                return 1;

        if (client->container->mode == MODE_STACK)
                render_container(conn, client->container);
        else decorate_window(conn, client, client->frame, client->titlegc, 0);
        xcb_flush(conn);

        return 1;
}

/*
 * We handle legacy window names (titles) which are in COMPOUND_TEXT encoding. However, we
 * just pass them along, so when containing non-ASCII characters, those will be rendering
 * incorrectly. In order to correctly render unicode window titles in i3, an application
 * has to set _NET_WM_NAME, which is in UTF-8 encoding.
 *
 * On every update, a message is put out to the user, so he may improve the situation and
 * update applications which display filenames in their title to correctly use
 * _NET_WM_NAME and therefore support unicode.
 *
 */
int handle_windowname_change_legacy(void *data, xcb_connection_t *conn, uint8_t state,
                                xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *prop) {
        LOG("window's name changed (legacy).\n");
        if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
                LOG("prop == NULL\n");
                return 1;
        }
        Client *client = table_get(byChild, window);
        if (client == NULL)
                return 1;

        if (client->uses_net_wm_name) {
                LOG("This client is capable of _NET_WM_NAME, ignoring legacy name\n");
                return 1;
        }

        /* Save the old pointer to make the update atomic */
        char *new_name;
        asprintf(&new_name, "%.*s", xcb_get_property_value_length(prop), (char*)xcb_get_property_value(prop));
        /* Convert it to UCS-2 here for not having to convert it later every time we want to pass it to X */
        LOG("Name should change to \"%s\"\n", new_name);

        /* Check if they are the same and don’t update if so. */
        if (client->name != NULL &&
            strlen(new_name) == strlen(client->name) &&
            strcmp(client->name, new_name) == 0) {
                LOG("Name did not change, not updating\n");
                free(new_name);
                return 1;
        }

        LOG("Using legacy window title. Note that in order to get Unicode window titles in i3,"
            "the application has to set _NET_WM_NAME which is in UTF-8 encoding.\n");

        char *old_name = client->name;
        client->name = new_name;
        client->name_len = -1;

        if (old_name != NULL)
                free(old_name);

        /* If the client is a dock window, we don’t need to render anything */
        if (client->dock)
                return 1;

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
        /* event->count is the number of minimum remaining expose events for this window, so we
           skip all events but the last one */
        if (event->count != 0)
                return 1;
        LOG("window = %08x\n", event->window);

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

                /* …or one of the bars? */
                i3Screen *screen;
                TAILQ_FOREACH(screen, virtual_screens, screens)
                        if (screen->bar == event->window)
                                render_layout(conn);
                return 1;
        }

        LOG("got client %s\n", client->name);
        if (client->dock) {
                LOG("this is a dock\n");
                return 1;
        }

        if (client->container->mode != MODE_STACK)
                decorate_window(conn, client, client->frame, client->titlegc, 0);
        else {
                uint32_t background_color;
                /* Distinguish if the window is currently focused… */
                if (CUR_CELL->currently_focused == client)
                        background_color = get_colorpixel(conn, "#285577");
                /* …or if it is the focused window in a not focused container */
                else background_color = get_colorpixel(conn, "#555555");

                /* Set foreground color to current focused color, line width to 2 */
                uint32_t values[] = {background_color, 2};
                xcb_change_gc(conn, client->titlegc, XCB_GC_FOREGROUND | XCB_GC_LINE_WIDTH, values);

                /* Draw the border, the ±1 is for line width = 2 */
                xcb_point_t points[] = {{1, 0},                                           /* left upper edge */
                                        {1, client->rect.height-1},                       /* left bottom edge */
                                        {client->rect.width-1, client->rect.height-1},    /* right bottom edge */
                                        {client->rect.width-1, 0}};                       /* right upper edge */
                xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, client->frame, client->titlegc, 4, points);

                /* Draw a black background */
                xcb_change_gc_single(conn, client->titlegc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#000000"));
                xcb_rectangle_t crect = {2, 0, client->rect.width - (2 + 2), client->rect.height - 2};
                xcb_poly_fill_rectangle(conn, client->frame, client->titlegc, 1, &crect);
        }
        xcb_flush(conn);
        return 1;
}

/*
 * Handle client messages (EWMH)
 *
 */
int handle_client_message(void *data, xcb_connection_t *conn, xcb_client_message_event_t *event) {
        LOG("client_message\n");

        if (event->type == atoms[_NET_WM_STATE]) {
                if (event->format != 32 || event->data.data32[1] != atoms[_NET_WM_STATE_FULLSCREEN])
                        return 0;

                LOG("fullscreen\n");

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
                LOG("unhandled clientmessage\n");
                return 0;
        }

        return 1;
}

int handle_window_type(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                        xcb_atom_t atom, xcb_get_property_reply_t *property) {
        /* TODO: Implement this one. To do this, implement a little test program which sleep(1)s
         before changing this property. */
        LOG("_NET_WM_WINDOW_TYPE changed, this is not yet implemented.\n");
        return 0;
}

/*
 * Handles the size hints set by a window, but currently only the part necessary for displaying
 * clients proportionally inside their frames (mplayer for example)
 *
 * See ICCCM 4.1.2.3 for more details
 *
 */
int handle_normal_hints(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                        xcb_atom_t name, xcb_get_property_reply_t *reply) {
        LOG("handle_normal_hints\n");
        Client *client = table_get(byChild, window);
        if (client == NULL) {
                LOG("No such client\n");
                return 1;
        }
        xcb_size_hints_t size_hints;

        /* If the hints were already in this event, use them, if not, request them */
        if (reply != NULL)
                xcb_get_wm_size_hints_from_reply(&size_hints, reply);
        else
                xcb_get_wm_normal_hints_reply(conn, xcb_get_wm_normal_hints_unchecked(conn, client->child), &size_hints, NULL);

        /* If no aspect ratio was set or if it was invalid, we ignore the hints */
        if (!(size_hints.flags & XCB_SIZE_HINT_P_ASPECT) ||
            (size_hints.min_aspect_num <= 0) ||
            (size_hints.min_aspect_den <= 0)) {
                LOG("No aspect ratio set, ignoring\n");
                return 1;
        }

        LOG("window is %08x / %s\n", client->child, client->name);

        int base_width = 0, base_height = 0,
            min_width = 0, min_height = 0;

        /* base_width/height are the desired size of the window.
           We check if either the program-specified size or the program-specified
           min-size is available */
        if (size_hints.flags & XCB_SIZE_HINT_P_SIZE) {
                base_width = size_hints.base_width;
                base_height = size_hints.base_height;
        } else if (size_hints.flags & XCB_SIZE_HINT_P_MIN_SIZE) {
                base_width = size_hints.min_width;
                base_height = size_hints.min_height;
        }

        if (size_hints.flags & XCB_SIZE_HINT_P_MIN_SIZE) {
                min_width = size_hints.min_width;
                min_height = size_hints.min_height;
        } else if (size_hints.flags & XCB_SIZE_HINT_P_SIZE) {
                min_width = size_hints.base_width;
                min_height = size_hints.base_height;
        }

        double width = client->rect.width - base_width;
        double height = client->rect.height - base_height;
        /* Convert numerator/denominator to a double */
        double min_aspect = (double)size_hints.min_aspect_num / size_hints.min_aspect_den;
        double max_aspect = (double)size_hints.max_aspect_num / size_hints.min_aspect_den;

        LOG("min_aspect = %f, max_aspect = %f\n", min_aspect, max_aspect);
        LOG("width = %f, height = %f\n", width, height);

        /* Sanity checks, this is user-input, in a way */
        if (max_aspect <= 0 || min_aspect <= 0 || height == 0 || (width / height) <= 0)
                return 1;

        /* Check if we need to set proportional_* variables using the correct ratio */
        if ((width / height) < min_aspect) {
                client->proportional_width = width;
                client->proportional_height = width / min_aspect;
        } else if ((width / height) > max_aspect) {
                client->proportional_width = width;
                client->proportional_height = width / max_aspect;
        } else return 1;

        client->force_reconfigure = true;

        if (client->container != NULL) {
                render_container(conn, client->container);
                xcb_flush(conn);
        }

        return 1;
}
