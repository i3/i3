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
#include "font.h"
#include "xcb.h"
#include "util.h"
#include "xinerama.h"

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
 * There was a key press. We lookup the key symbol and see if there are any bindings
 * on that. This allows to do things like binding special characters (think of ä) to
 * functions to get one more modifier while not losing AltGr :-)
 * TODO: this description needs to be more understandable
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

        /* Find the binding */
        /* TODO: event->state durch eine bitmask filtern und dann direkt vergleichen */
        Binding *bind, *best_match = TAILQ_END(&bindings);
        TAILQ_FOREACH(bind, &bindings, bindings) {
                if (bind->keycode == event->detail &&
                        (bind->mods & event->state) == bind->mods) {
                        if (best_match == TAILQ_END(&bindings) ||
                                bind->mods > best_match->mods)
                                best_match = bind;
                }
        }

        /* No match? Then it was an actively grabbed key, that is with Mode_switch, and
           the user did not press Mode_switch, so just pass it… */
        if (best_match == TAILQ_END(&bindings)) {
                xcb_allow_events(conn, ReplayKeyboard, event->time);
                xcb_flush(conn);
                return 1;
        }

        if (event->state & 0x2) {
                printf("that's mode_switch\n");
                parse_command(conn, best_match->command);
                printf("ok, hiding this event.\n");
                xcb_allow_events(conn, SyncKeyboard, event->time);
                xcb_flush(conn);
                return 1;
        }

        parse_command(conn, best_match->command);
        return 1;
}


/*
 * When the user moves the mouse pointer onto a window, this callback gets called.
 *
 */
int handle_enter_notify(void *ignored, xcb_connection_t *conn, xcb_enter_notify_event_t *event) {
        printf("enter_notify\n");

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
        if (client == NULL)
                client = table_get(byParent, event->event);
        if (client == NULL)
                return 1;

        xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;
        xcb_screen_t *root_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

        /* Set focus in any case */
        set_focus(conn, client);

        /* Let’s see if this was on the borders (= resize). If not, we’re done */
        i3Font *font = load_font(conn, pattern);
        printf("press button on x=%d, y=%d\n", event->event_x, event->event_y);
        if (event->event_y <= (font->height + 2))
                return 1;

        printf("that was resize\n");

        /* Open a new window, the resizebar. Grab the pointer and move the window around
           as the user moves the pointer. */


        /* TODO: the whole logic is missing. this is just a proof of concept */
        xcb_window_t grabwin = xcb_generate_id(conn);

        uint32_t mask = 0;
        uint32_t values[3];

        xcb_create_window(conn,
                        0,
                        grabwin,
                        root,
                        0, /* x */
                        0, /* y */
                        root_screen->width_in_pixels, /* width */
                        root_screen->height_in_pixels, /* height */
                        /* border_width */ 0,
                        XCB_WINDOW_CLASS_INPUT_ONLY,
                        root_screen->root_visual,
                        0,
                        values);

        /* Map the window on the screen (= make it visible) */
        xcb_map_window(conn, grabwin);

        xcb_window_t helpwin = xcb_generate_id(conn);

        mask = XCB_CW_BACK_PIXEL;
        values[0] = root_screen->white_pixel;
        xcb_create_window(conn, root_screen->root_depth, helpwin, root,
                        event->root_x,
                        0,
                        5,
                        root_screen->height_in_pixels,
                        /* bordor */ 0,
                        XCB_WINDOW_CLASS_INPUT_OUTPUT,
                        root_screen->root_visual,
                        mask,
                        values);

        xcb_map_window(conn, helpwin);
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
                        handle_event(NULL, conn, inside_event);
                        continue;
                }
                assert(nr < 256);
                nr &= XCB_EVENT_RESPONSE_TYPE_MASK;
                assert(nr >= 2);

                /* Check if we need to escape this loop… */
                if (nr == XCB_BUTTON_RELEASE)
                        break;

                switch (nr) {
                        case XCB_MOTION_NOTIFY:
                                values[0] = ((xcb_motion_notify_event_t*)inside_event)->root_x;
                                xcb_configure_window(conn, helpwin, XCB_CONFIG_WINDOW_X, values);
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
                printf("---\n");
                free(inside_event);
        }

        xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
        xcb_destroy_window(conn, helpwin);
        xcb_destroy_window(conn, grabwin);
        xcb_flush(conn);

        return 1;
}

int handle_map_notify_event(void *prophs, xcb_connection_t *conn, xcb_map_notify_event_t *event) {
        window_attributes_t wa = { TAG_VALUE };
        wa.u.override_redirect = event->override_redirect;
        printf("MapNotify for 0x%08x.\n", event->window);
        manage_window(prophs, conn, event->window, wa);
        return 1;
}

/*
 * Our window decorations were unmapped. That means, the window will be killed now,
 * so we better clean up before.
 *
 */
int handle_unmap_notify_event(void *data, xcb_connection_t *c, xcb_unmap_notify_event_t *e) {
        xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(c)).data->root;

        Client *client = table_get(byChild, e->window);
        /* First, we need to check if the client is awaiting an unmap-request which
           was generated by us reparenting the window. In that case, we just ignore it. */
        if (client != NULL && client->awaiting_useless_unmap) {
                printf("Dropping this unmap request, it was generated by reparenting\n");
                client->awaiting_useless_unmap = false;
                return 1;
        }
        client = table_remove(byChild, e->window);

        printf("UnmapNotify for 0x%08x (received from 0x%08x): ", e->window, e->event);
        if(client == NULL) {
                printf("not a managed window. Ignoring.\n");
                return 0;
        }


        if (client->container->currently_focused == client)
                client->container->currently_focused = NULL;
        CIRCLEQ_REMOVE(&(client->container->clients), client, clients);

        printf("child of 0x%08x.\n", client->frame);
        xcb_reparent_window(c, client->child, root, 0, 0);
        xcb_destroy_window(c, client->frame);
        xcb_flush(c);
        table_remove(byParent, client->frame);
        free(client);

        render_layout(c);

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

        client->name_len = xcb_get_property_value_length(prop);
        client->name = malloc(client->name_len);
        strncpy(client->name, xcb_get_property_value(prop), client->name_len);
        printf("rename to \"%.*s\".\n", client->name_len, client->name);

        decorate_window(conn, client);
        xcb_flush(conn);

        return 1;
}

/*
 * Expose event means we should redraw our windows (= title bar)
 *
 */
int handle_expose_event(void *data, xcb_connection_t *conn, xcb_expose_event_t *e) {
        printf("handle_expose_event()\n");
        Client *client = table_get(byParent, e->window);
        if(!client || e->count != 0)
                return 1;
        decorate_window(conn, client);
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

                /* Check if the fullscreen state should be toggled… */
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
