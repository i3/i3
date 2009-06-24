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
#include <xcb/xcb_atom.h>
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
#include "resize.h"
#include "client.h"
#include "manage.h"
#include "floating.h"

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
        LOG("Keypress %d, state raw = %d\n", event->detail, event->state);

        /* Remove the numlock bit, all other bits are modifiers we can bind to */
        uint16_t state_filtered = event->state & ~(xcb_numlock_mask | XCB_MOD_MASK_LOCK);
        LOG("(removed numlock, state = %d)\n", state_filtered);
        /* Only use the lower 8 bits of the state (modifier masks) so that mouse
         * button masks are filtered out */
        state_filtered &= 0xFF;
        LOG("(removed upper 8 bits, state = %d)\n", state_filtered);

        /* We need to get the keysym group (There are group 1 to group 4, each holding
           two keysyms (without shift and with shift) using Xkb because X fails to
           provide them reliably (it works in Xephyr, it does not in real X) */
        XkbStateRec state;
        if (XkbGetState(xkbdpy, XkbUseCoreKbd, &state) == Success && (state.group+1) == 2)
                state_filtered |= BIND_MODE_SWITCH;

        LOG("(checked mode_switch, state %d)\n", state_filtered);

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
        if (state_filtered & BIND_MODE_SWITCH) {
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
           user, but by reconfiguration of windows */
        if (event_is_ignored(event->sequence))
                return 1;

        /* This was either a focus for a client’s parent (= titlebar)… */
        Client *client = table_get(&by_parent, event->event);
        /* …or the client itself */
        if (client == NULL)
                client = table_get(&by_child, event->event);

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

        if (client->workspace != c_ws && client->workspace->screen == c_ws->screen) {
                /* This can happen when a client gets assigned to a different workspace than
                 * the current one (see src/mainx.c:reparent_window). Shortly after it was created,
                 * an enter_notify will follow. */
                LOG("enter_notify for a client on a different workspace but the same screen, ignoring\n");
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
        LOG("state = %d\n", event->state);
        /* This was either a focus for a client’s parent (= titlebar)… */
        Client *client = table_get(&by_child, event->event);
        bool border_click = false;
        if (client == NULL) {
                client = table_get(&by_parent, event->event);
                border_click = true;
        }
        /* See if this was a click with Mod1. If so, we need to move around
         * the client if it was floating. if not, we just process as usual. */
        if ((event->state & XCB_MOD_MASK_1) != 0) {
                if (client == NULL) {
                        LOG("Not handling, Mod1 was pressed and no client found\n");
                        return 1;
                }
                if (client_is_floating(client)) {
                        floating_drag_window(conn, client, event);
                        return 1;
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

        Client *client = table_get(&by_child, event->window);
        if (client == NULL) {
                LOG("This client is not mapped, so we don't care and just tell the client that he will get its size\n");
                uint32_t mask = 0;
                uint32_t values[7];
                int c = 0;
#define COPY_MASK_MEMBER(mask_member, event_member) do { \
                if (event->value_mask & mask_member) { \
                        mask |= mask_member; \
                        values[c++] = event->event_member; \
                } \
} while (0)

                COPY_MASK_MEMBER(XCB_CONFIG_WINDOW_X, x);
                COPY_MASK_MEMBER(XCB_CONFIG_WINDOW_Y, y);
                COPY_MASK_MEMBER(XCB_CONFIG_WINDOW_WIDTH, width);
                COPY_MASK_MEMBER(XCB_CONFIG_WINDOW_HEIGHT, height);
                COPY_MASK_MEMBER(XCB_CONFIG_WINDOW_BORDER_WIDTH, border_width);
                COPY_MASK_MEMBER(XCB_CONFIG_WINDOW_SIBLING, sibling);
                COPY_MASK_MEMBER(XCB_CONFIG_WINDOW_STACK_MODE, stack_mode);

                xcb_configure_window(conn, event->window, mask, values);
                xcb_flush(conn);

                return 1;
        }

        /* Floating clients can be reconfigured */
        if (client_is_floating(client)) {
                i3Font *font = load_font(conn, config.font);

                if (event->value_mask & XCB_CONFIG_WINDOW_X)
                        client->rect.x = event->x;
                if (event->value_mask & XCB_CONFIG_WINDOW_Y)
                        client->rect.y = event->y;
                if (event->value_mask & XCB_CONFIG_WINDOW_WIDTH)
                        client->rect.width = event->width + 2 + 2;
                if (event->value_mask & XCB_CONFIG_WINDOW_HEIGHT)
                        client->rect.height = event->height + (font->height + 2 + 2) + 2;

                LOG("Accepted new position/size for floating client: (%d, %d) size %d x %d\n",
                    client->rect.x, client->rect.y, client->rect.width, client->rect.height);

                /* Push the new position/size to X11 */
                reposition_client(conn, client);
                resize_client(conn, client);
                xcb_flush(conn);

                return 1;
        }

        if (client->fullscreen) {
                LOG("Client is in fullscreen mode\n");

                Rect child_rect = client->container->workspace->rect;
                child_rect.x = child_rect.y = 0;
                fake_configure_notify(conn, child_rect, client->child);

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

        /* We ignore this sequence twice because events for child and frame should be ignored */
        add_ignore_event(event->sequence);
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

        Client *client = table_get(&by_child, event->window);
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

                /* This was most likely the destroyed frame of a client which is
                 * currently being unmapped, so we add this sequence (again!) to
                 * the ignore list (enter_notify events will get sent for both,
                 * the child and its frame). */
                add_ignore_event(event->sequence);

                return 0;
        }

        client = table_remove(&by_child, event->window);

        /* Clients without a container are either floating or dock windows */
        if (client->container != NULL) {
                Container *con = client->container;

                /* If this was the fullscreen client, we need to unset it */
                if (client->fullscreen)
                        con->workspace->fullscreen_client = NULL;

                /* Remove the client from the list of clients */
                client_remove_from_container(conn, client, con, true);

                /* Set focus to the last focused client in this container */
                con->currently_focused = get_last_focused_client(conn, con, NULL);

                /* Only if this is the active container, we need to really change focus */
                if ((con->currently_focused != NULL) && ((con == CUR_CELL) || client->fullscreen))
                        set_focus(conn, con->currently_focused, true);
        } else if (client_is_floating(client)) {
                SLIST_REMOVE(&(client->workspace->focus_stack), client, Client, focus_clients);
        }

        if (client->dock) {
                LOG("Removing from dock clients\n");
                SLIST_REMOVE(&(client->workspace->screen->dock_clients), client, Client, dock_clients);
        }

        if (client->floating) {
                LOG("Removing from floating clients\n");
                TAILQ_REMOVE(&(client->workspace->floating_clients), client, floating_clients);
        }

        LOG("child of 0x%08x.\n", client->frame);
        xcb_reparent_window(conn, client->child, root, 0, 0);
        xcb_destroy_window(conn, client->frame);
        xcb_flush(conn);
        table_remove(&by_parent, client->frame);

        if (client->container != NULL) {
                Workspace *workspace = client->container->workspace;
                cleanup_table(conn, workspace);
                fix_colrowspan(conn, workspace);
        }

        /* Let’s see how many clients there are left on the workspace to delete it if it’s empty */
        bool workspace_empty = SLIST_EMPTY(&(client->workspace->focus_stack));
        bool workspace_active = false;
        Client *to_focus = (!workspace_empty ? SLIST_FIRST(&(client->workspace->focus_stack)) : NULL);

        /* If this workspace is currently active, we don’t delete it */
        i3Screen *screen;
        TAILQ_FOREACH(screen, virtual_screens, screens)
                if (screen->current_workspace == client->workspace->num) {
                        workspace_active = true;
                        workspace_empty = false;
                        break;
                }

        if (workspace_empty) {
                LOG("setting ws to NULL for workspace %d (%p)\n", client->workspace->num,
                                client->workspace);
                client->workspace->screen = NULL;
        }

        FREE(client->window_class);
        FREE(client->name);
        free(client);

        render_layout(conn);

        /* Ensure the focus is set to the next client in the focus stack */
        if (workspace_active && to_focus != NULL)
                set_focus(conn, to_focus, true);

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
        Client *client = table_get(&by_child, window);
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

        FREE(old_name);

        /* If the client is a dock window, we don’t need to render anything */
        if (client->dock)
                return 1;

        if (client->container != NULL && client->container->mode == MODE_STACK)
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
        Client *client = table_get(&by_child, window);
        if (client == NULL)
                return 1;

        if (client->uses_net_wm_name) {
                LOG("This client is capable of _NET_WM_NAME, ignoring legacy name\n");
                return 1;
        }

        /* Save the old pointer to make the update atomic */
        char *new_name;
        if (asprintf(&new_name, "%.*s", xcb_get_property_value_length(prop), (char*)xcb_get_property_value(prop)) == -1) {
                perror("Could not get old name");
                LOG("Could not get old name\n");
                return 1;
        }
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

        if (client->container != NULL && client->container->mode == MODE_STACK)
                render_container(conn, client->container);
        else decorate_window(conn, client, client->frame, client->titlegc, 0);
        xcb_flush(conn);

        return 1;
}

/*
 * Updates the client’s WM_CLASS property
 *
 */
int handle_windowclass_change(void *data, xcb_connection_t *conn, uint8_t state,
                             xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *prop) {
        LOG("window class changed\n");
        if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
                LOG("prop == NULL\n");
                return 1;
        }
        Client *client = table_get(&by_child, window);
        if (client == NULL)
                return 1;
        char *new_class;
        if (asprintf(&new_class, "%.*s", xcb_get_property_value_length(prop), (char*)xcb_get_property_value(prop)) == -1) {
                perror("Could not get window class");
                LOG("Could not get window class\n");
                return 1;
        }

        LOG("changed to %s\n", new_class);
        char *old_class = client->window_class;
        client->window_class = new_class;
        FREE(old_class);

        if (!client->initialized) {
                LOG("Client is not yet initialized, not putting it to floating\n");
                return 1;
        }

        if (strcmp(new_class, "tools") == 0 || strcmp(new_class, "Dialog") == 0) {
                LOG("tool/dialog window, should we put it floating?\n");
                if (client->floating == FLOATING_AUTO_OFF)
                        toggle_floating_mode(conn, client, true);
        }

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

        Client *client = table_get(&by_parent, event->window);
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

        if (client->container == NULL || client->container->mode != MODE_STACK)
                decorate_window(conn, client, client->frame, client->titlegc, 0);
        else {
                uint32_t background_color;
                /* Distinguish if the window is currently focused… */
                if (CUR_CELL->currently_focused == client)
                        background_color = config.client.focused.background;
                /* …or if it is the focused window in a not focused container */
                else background_color = config.client.focused_inactive.background;

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

                Client *client = table_get(&by_child, event->window);
                if (client == NULL)
                        return 0;

                /* Check if the fullscreen state should be toggled */
                if ((client->fullscreen &&
                     (event->data.data32[0] == _NET_WM_STATE_REMOVE ||
                      event->data.data32[0] == _NET_WM_STATE_TOGGLE)) ||
                    (!client->fullscreen &&
                     (event->data.data32[0] == _NET_WM_STATE_ADD ||
                      event->data.data32[0] == _NET_WM_STATE_TOGGLE)))
                        client_toggle_fullscreen(conn, client);
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
        Client *client = table_get(&by_child, window);
        if (client == NULL) {
                LOG("No such client\n");
                return 1;
        }
        xcb_size_hints_t size_hints;
        LOG("client is %08x / child %08x\n", client->frame, client->child);

        /* If the hints were already in this event, use them, if not, request them */
        if (reply != NULL)
                xcb_get_wm_size_hints_from_reply(&size_hints, reply);
        else
                xcb_get_wm_normal_hints_reply(conn, xcb_get_wm_normal_hints_unchecked(conn, client->child), &size_hints, NULL);

        if ((size_hints.flags & XCB_SIZE_HINT_P_MIN_SIZE)) {
                LOG("min size set\n");
                LOG("gots min_width = %d, min_height = %d\n", size_hints.min_width, size_hints.min_height);
        }

        /* If no aspect ratio was set or if it was invalid, we ignore the hints */
        if (!(size_hints.flags & XCB_SIZE_HINT_P_ASPECT) ||
            (size_hints.min_aspect_num <= 0) ||
            (size_hints.min_aspect_den <= 0)) {
                LOG("No aspect ratio set, ignoring\n");
                return 1;
        }

        LOG("window is %08x / %s\n", client->child, client->name);

        int base_width = 0, base_height = 0;

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

/*
 * Handles the transient for hints set by a window, signalizing that this window is a popup window
 * for some other window.
 *
 * See ICCCM 4.1.2.6 for more details
 *
 */
int handle_transient_for(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                         xcb_atom_t name, xcb_get_property_reply_t *reply) {
        LOG("Transient hint!\n");
        Client *client = table_get(&by_child, window);
        if (client == NULL) {
                LOG("No such client\n");
                return 1;
        }

        xcb_window_t transient_for;

        if (reply != NULL) {
                if (!xcb_get_wm_transient_for_from_reply(&transient_for, reply)) {
                        LOG("Not transient for any window\n");
                        return 1;
                }
        } else {
                if (!xcb_get_wm_transient_for_reply(conn, xcb_get_wm_transient_for_unchecked(conn, window),
                                                    &transient_for, NULL)) {
                        LOG("Not transient for any window\n");
                        return 1;
                }
        }

        if (client->floating == FLOATING_AUTO_OFF) {
                LOG("This is a popup window, putting into floating\n");
                toggle_floating_mode(conn, client, true);
        }

        return 1;
}
