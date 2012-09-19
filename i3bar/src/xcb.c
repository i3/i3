/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2012 Axel Wagner and contributors (see also: LICENSE)
 *
 * xcb.c: Communicating with X
 *
 */
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xcb/xcb_atom.h>
#include <xcb/xcb_aux.h>

#ifdef XCB_COMPAT
#include "xcb_compat.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <i3/ipc.h>
#include <ev.h>
#include <errno.h>
#include <limits.h>
#include <err.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKB.h>

#include "common.h"
#include "libi3.h"

/* We save the Atoms in an easy to access array, indexed by an enum */
enum {
    #define ATOM_DO(name) name,
    #include "xcb_atoms.def"
    NUM_ATOMS
};

xcb_intern_atom_cookie_t atom_cookies[NUM_ATOMS];
xcb_atom_t               atoms[NUM_ATOMS];

/* Variables, that are the same for all functions at all times */
xcb_connection_t *xcb_connection;
int              screen;
xcb_screen_t     *root_screen;
xcb_window_t     xcb_root;

/* This is needed for integration with libi3 */
xcb_connection_t *conn;

/* The font we'll use */
static i3Font font;

/* These are only relevant for XKB, which we only need for grabbing modifiers */
Display          *xkb_dpy;
int              xkb_event_base;
int              mod_pressed = 0;

/* Because the statusline is the same on all outputs, we have
 * global buffer to render it on */
xcb_gcontext_t   statusline_ctx;
xcb_gcontext_t   statusline_clear;
xcb_pixmap_t     statusline_pm;
uint32_t         statusline_width;

/* Event-Watchers, to interact with the user */
ev_prepare *xcb_prep;
ev_check   *xcb_chk;
ev_io      *xcb_io;
ev_io      *xkb_io;

/* The parsed colors */
struct xcb_colors_t {
    uint32_t bar_fg;
    uint32_t bar_bg;
    uint32_t active_ws_fg;
    uint32_t active_ws_bg;
    uint32_t active_ws_border;
    uint32_t inactive_ws_fg;
    uint32_t inactive_ws_bg;
    uint32_t inactive_ws_border;
    uint32_t urgent_ws_bg;
    uint32_t urgent_ws_fg;
    uint32_t urgent_ws_border;
    uint32_t focus_ws_bg;
    uint32_t focus_ws_fg;
    uint32_t focus_ws_border;
};
struct xcb_colors_t colors;

/* We define xcb_request_failed as a macro to include the relevant line-number */
#define xcb_request_failed(cookie, err_msg) _xcb_request_failed(cookie, err_msg, __LINE__)
int _xcb_request_failed(xcb_void_cookie_t cookie, char *err_msg, int line) {
    xcb_generic_error_t *err;
    if ((err = xcb_request_check(xcb_connection, cookie)) != NULL) {
        fprintf(stderr, "[%s:%d] ERROR: %s. X Error Code: %d\n", __FILE__, line, err_msg, err->error_code);
        return err->error_code;
    }
    return 0;
}

/*
 * Redraws the statusline to the buffer
 *
 */
void refresh_statusline(void) {
    struct status_block *block;

    uint32_t old_statusline_width = statusline_width;
    statusline_width = 0;

    /* Predict the text width of all blocks (in pixels). */
    TAILQ_FOREACH(block, &statusline_head, blocks) {
        if (i3string_get_num_bytes(block->full_text) == 0)
            continue;

        block->width = predict_text_width(block->full_text);
        /* If this is not the last block, add some pixels for a separator. */
        if (TAILQ_NEXT(block, blocks) != NULL)
            block->width += 9;
        statusline_width += block->width;
    }

    /* If the statusline is bigger than our screen we need to make sure that
     * the pixmap provides enough space, so re-allocate if the width grew */
    if (statusline_width > root_screen->width_in_pixels &&
        statusline_width > old_statusline_width)
        realloc_sl_buffer();

    /* Clear the statusline pixmap. */
    xcb_rectangle_t rect = { 0, 0, root_screen->width_in_pixels, font.height };
    xcb_poly_fill_rectangle(xcb_connection, statusline_pm, statusline_clear, 1, &rect);

    /* Draw the text of each block. */
    uint32_t x = 0;
    TAILQ_FOREACH(block, &statusline_head, blocks) {
        if (i3string_get_num_bytes(block->full_text) == 0)
            continue;

        uint32_t colorpixel = (block->color ? get_colorpixel(block->color) : colors.bar_fg);
        set_font_colors(statusline_ctx, colorpixel, colors.bar_bg);
        draw_text(block->full_text, statusline_pm, statusline_ctx, x, 0, block->width);
        x += block->width;

        if (TAILQ_NEXT(block, blocks) != NULL) {
            /* This is not the last block, draw a separator. */
            set_font_colors(statusline_ctx, get_colorpixel("#666666"), colors.bar_bg);
            xcb_poly_line(xcb_connection, XCB_COORD_MODE_ORIGIN, statusline_pm,
                          statusline_ctx, 2,
                          (xcb_point_t[]){ { x - 5, 2 }, { x - 5, font.height - 2 } });
        }
    }
}

/*
 * Hides all bars (unmaps them)
 *
 */
void hide_bars(void) {
    if (!config.hide_on_modifier) {
        return;
    }

    i3_output *walk;
    SLIST_FOREACH(walk, outputs, slist) {
        if (!walk->active) {
            continue;
        }
        xcb_unmap_window(xcb_connection, walk->bar);
    }
    stop_child();
}

/*
 * Unhides all bars (maps them)
 *
 */
void unhide_bars(void) {
    if (!config.hide_on_modifier) {
        return;
    }

    i3_output           *walk;
    xcb_void_cookie_t   cookie;
    uint32_t            mask;
    uint32_t            values[5];

    cont_child();

    SLIST_FOREACH(walk, outputs, slist) {
        if (walk->bar == XCB_NONE) {
            continue;
        }
        mask = XCB_CONFIG_WINDOW_X |
               XCB_CONFIG_WINDOW_Y |
               XCB_CONFIG_WINDOW_WIDTH |
               XCB_CONFIG_WINDOW_HEIGHT |
               XCB_CONFIG_WINDOW_STACK_MODE;
        values[0] = walk->rect.x;
        if (config.position == POS_TOP)
            values[1] = walk->rect.y;
        else values[1] = walk->rect.y + walk->rect.h - font.height - 6;
        values[2] = walk->rect.w;
        values[3] = font.height + 6;
        values[4] = XCB_STACK_MODE_ABOVE;
        DLOG("Reconfiguring Window for output %s to %d,%d\n", walk->name, values[0], values[1]);
        cookie = xcb_configure_window_checked(xcb_connection,
                                              walk->bar,
                                              mask,
                                              values);

        if (xcb_request_failed(cookie, "Could not reconfigure window")) {
            exit(EXIT_FAILURE);
        }
        xcb_map_window(xcb_connection, walk->bar);
    }
}

/*
 * Parse the colors into a format that we can use
 *
 */
void init_colors(const struct xcb_color_strings_t *new_colors) {
#define PARSE_COLOR(name, def) \
    do { \
        colors.name = get_colorpixel(new_colors->name ? new_colors->name : def); \
    } while  (0)
    PARSE_COLOR(bar_fg, "#FFFFFF");
    PARSE_COLOR(bar_bg, "#000000");
    PARSE_COLOR(active_ws_fg, "#FFFFFF");
    PARSE_COLOR(active_ws_bg, "#333333");
    PARSE_COLOR(active_ws_border, "#333333");
    PARSE_COLOR(inactive_ws_fg, "#888888");
    PARSE_COLOR(inactive_ws_bg, "#222222");
    PARSE_COLOR(inactive_ws_border, "#333333");
    PARSE_COLOR(urgent_ws_fg, "#FFFFFF");
    PARSE_COLOR(urgent_ws_bg, "#900000");
    PARSE_COLOR(urgent_ws_border, "#2f343a");
    PARSE_COLOR(focus_ws_fg, "#FFFFFF");
    PARSE_COLOR(focus_ws_bg, "#285577");
    PARSE_COLOR(focus_ws_border, "#4c7899");
#undef PARSE_COLOR
}

/*
 * Handle a button-press-event (i.e. a mouse click on one of our bars).
 * We determine, whether the click occured on a ws-button or if the scroll-
 * wheel was used and change the workspace appropriately
 *
 */
void handle_button(xcb_button_press_event_t *event) {
    i3_ws *cur_ws;

    /* Determine, which bar was clicked */
    i3_output *walk;
    xcb_window_t bar = event->event;
    SLIST_FOREACH(walk, outputs, slist) {
        if (walk->bar == bar) {
            break;
        }
    }

    if (walk == NULL) {
        DLOG("Unknown Bar klicked!\n");
        return;
    }

    /* TODO: Move this to extern get_ws_for_output() */
    TAILQ_FOREACH(cur_ws, walk->workspaces, tailq) {
        if (cur_ws->visible) {
            break;
        }
    }

    if (cur_ws == NULL) {
        DLOG("No Workspace active?\n");
        return;
    }

    int32_t x = event->event_x >= 0 ? event->event_x : 0;

    DLOG("Got Button %d\n", event->detail);

    switch (event->detail) {
        case 1:
            /* Left Mousbutton. We determine, which button was clicked
             * and set cur_ws accordingly */
            TAILQ_FOREACH(cur_ws, walk->workspaces, tailq) {
                DLOG("x = %d\n", x);
                if (x >= 0 && x < cur_ws->name_width + 10) {
                    break;
                }
                x -= cur_ws->name_width + 11;
            }
            if (cur_ws == NULL) {
                return;
            }
            break;
        case 4:
            /* Mouse wheel down. We select the next ws */
            if (cur_ws != TAILQ_FIRST(walk->workspaces)) {
                cur_ws = TAILQ_PREV(cur_ws, ws_head, tailq);
            }
            break;
        case 5:
            /* Mouse wheel up. We select the previos ws */
            if (cur_ws != TAILQ_LAST(walk->workspaces, ws_head)) {
                cur_ws = TAILQ_NEXT(cur_ws, tailq);
            }
            break;
    }

    /* To properly handle workspace names with double quotes in them, we need
     * to escape the double quotes. Unfortunately, that’s rather ugly in C: We
     * first count the number of double quotes, then we allocate a large enough
     * buffer, then we copy character by character. */
    int num_quotes = 0;
    size_t namelen = 0;
    const char *utf8_name = i3string_as_utf8(cur_ws->name);
    for (const char *walk = utf8_name; *walk != '\0'; walk++) {
        if (*walk == '"')
            num_quotes++;
        /* While we’re looping through the name anyway, we can save one
         * strlen(). */
        namelen++;
    }

    const size_t len = namelen + strlen("workspace \"\"") + 1;
    char *buffer = scalloc(len+num_quotes);
    strncpy(buffer, "workspace \"", strlen("workspace \""));
    int inpos, outpos;
    for (inpos = 0, outpos = strlen("workspace \"");
         inpos < namelen;
         inpos++, outpos++) {
        if (utf8_name[inpos] == '"') {
            buffer[outpos] = '\\';
            outpos++;
        }
        buffer[outpos] = utf8_name[inpos];
    }
    buffer[outpos] = '"';
    i3_send_msg(I3_IPC_MESSAGE_TYPE_COMMAND, buffer);
    free(buffer);
}

/*
 * Configures the x coordinate of all trayclients. To be called after adding a
 * new tray client or removing an old one.
 *
 */
static void configure_trayclients(void) {
    trayclient *trayclient;
    i3_output *output;
    SLIST_FOREACH(output, outputs, slist) {
        if (!output->active)
            continue;

        int clients = 0;
        TAILQ_FOREACH_REVERSE(trayclient, output->trayclients, tc_head, tailq) {
            if (!trayclient->mapped)
                continue;
            clients++;

            DLOG("Configuring tray window %08x to x=%d\n",
                 trayclient->win, output->rect.w - (clients * (font.height + 2)));
            uint32_t x = output->rect.w - (clients * (font.height + 2));
            xcb_configure_window(xcb_connection,
                                 trayclient->win,
                                 XCB_CONFIG_WINDOW_X,
                                 &x);
        }
    }
}

/*
 * Handles ClientMessages (messages sent from another client directly to us).
 *
 * At the moment, only the tray window will receive client messages. All
 * supported client messages currently are _NET_SYSTEM_TRAY_OPCODE.
 *
 */
static void handle_client_message(xcb_client_message_event_t* event) {
    if (event->type == atoms[_NET_SYSTEM_TRAY_OPCODE] &&
        event->format == 32) {
        DLOG("_NET_SYSTEM_TRAY_OPCODE received\n");
        /* event->data.data32[0] is the timestamp */
        uint32_t op = event->data.data32[1];
        uint32_t mask;
        uint32_t values[2];
        if (op == SYSTEM_TRAY_REQUEST_DOCK) {
            xcb_window_t client = event->data.data32[2];

            /* Listen for PropertyNotify events to get the most recent value of
             * the XEMBED_MAPPED atom, also listen for UnmapNotify events */
            mask = XCB_CW_EVENT_MASK;
            values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE |
                        XCB_EVENT_MASK_STRUCTURE_NOTIFY;
            xcb_change_window_attributes(xcb_connection,
                                         client,
                                         mask,
                                         values);

            /* Request the _XEMBED_INFO property. The XEMBED specification
             * (which is referred by the tray specification) says this *has* to
             * be set, but VLC does not set it… */
            bool map_it = true;
            int xe_version = 1;
            xcb_get_property_cookie_t xembedc;
            xcb_generic_error_t *error;
            xembedc = xcb_get_property(xcb_connection,
                                       0,
                                       client,
                                       atoms[_XEMBED_INFO],
                                       XCB_GET_PROPERTY_TYPE_ANY,
                                       0,
                                       2 * 32);

            xcb_get_property_reply_t *xembedr = xcb_get_property_reply(xcb_connection,
                                                                       xembedc,
                                                                       &error);
            if (error != NULL) {
                ELOG("Error getting _XEMBED_INFO property: error_code %d\n",
                     error->error_code);
                free(error);
                return;
            }
            if (xembedr != NULL && xembedr->length != 0) {
                DLOG("xembed format = %d, len = %d\n", xembedr->format, xembedr->length);
                uint32_t *xembed = xcb_get_property_value(xembedr);
                DLOG("xembed version = %d\n", xembed[0]);
                DLOG("xembed flags = %d\n", xembed[1]);
                map_it = ((xembed[1] & XEMBED_MAPPED) == XEMBED_MAPPED);
                xe_version = xembed[0];
                if (xe_version > 1)
                    xe_version = 1;
                free(xembedr);
            } else {
                ELOG("Window %08x violates the XEMBED protocol, _XEMBED_INFO not set\n", client);
            }

            DLOG("X window %08x requested docking\n", client);
            i3_output *walk, *output = NULL;
            SLIST_FOREACH(walk, outputs, slist) {
                if (!walk->active)
                    continue;
                if (config.tray_output) {
                    if ((strcasecmp(walk->name, config.tray_output) != 0) &&
                        (!walk->primary || strcasecmp("primary", config.tray_output) != 0))
                        continue;
                }

                DLOG("using output %s\n", walk->name);
                output = walk;
                break;
            }
            /* In case of tray_output == primary and there is no primary output
             * configured, we fall back to the first available output. */
            if (output == NULL && strcasecmp("primary", config.tray_output) == 0) {
                SLIST_FOREACH(walk, outputs, slist) {
                    if (!walk->active)
                        continue;
                    DLOG("Falling back to output %s because no primary output is configured\n", walk->name);
                    output = walk;
                    break;
                }
            }
            if (output == NULL) {
                ELOG("No output found\n");
                return;
            }
            xcb_reparent_window(xcb_connection,
                                client,
                                output->bar,
                                output->rect.w - font.height - 2,
                                2);
            /* We reconfigure the window to use a reasonable size. The systray
             * specification explicitly says:
             *   Tray icons may be assigned any size by the system tray, and
             *   should do their best to cope with any size effectively
             */
            mask = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
            values[0] = font.height;
            values[1] = font.height;
            xcb_configure_window(xcb_connection,
                                 client,
                                 mask,
                                 values);

            /* send the XEMBED_EMBEDDED_NOTIFY message */
            void *event = scalloc(32);
            xcb_client_message_event_t *ev = event;
            ev->response_type = XCB_CLIENT_MESSAGE;
            ev->window = client;
            ev->type = atoms[_XEMBED];
            ev->format = 32;
            ev->data.data32[0] = XCB_CURRENT_TIME;
            ev->data.data32[1] = atoms[XEMBED_EMBEDDED_NOTIFY];
            ev->data.data32[2] = output->bar;
            ev->data.data32[3] = xe_version;
            xcb_send_event(xcb_connection,
                           0,
                           client,
                           XCB_EVENT_MASK_NO_EVENT,
                           (char*)ev);
            free(event);

            /* Put the client inside the save set. Upon termination (whether
             * killed or normal exit does not matter) of i3bar, these clients
             * will be correctly reparented to their most closest living
             * ancestor. Without this, tray icons might die when i3bar
             * exits/crashes. */
            xcb_change_save_set(xcb_connection, XCB_SET_MODE_INSERT, client);

            if (map_it) {
                DLOG("Mapping dock client\n");
                xcb_map_window(xcb_connection, client);
            } else {
                DLOG("Not mapping dock client yet\n");
            }
            trayclient *tc = smalloc(sizeof(trayclient));
            tc->win = client;
            tc->mapped = map_it;
            tc->xe_version = xe_version;
            TAILQ_INSERT_TAIL(output->trayclients, tc, tailq);

            /* Trigger an update to copy the statusline text to the appropriate
             * position */
            configure_trayclients();
            draw_bars();
        }
    }
}

/*
 * Handles UnmapNotify events. These events happen when a tray window unmaps
 * itself. We then update our data structure
 *
 */
static void handle_unmap_notify(xcb_unmap_notify_event_t* event) {
    DLOG("UnmapNotify for window = %08x, event = %08x\n", event->window, event->event);

    i3_output *walk;
    SLIST_FOREACH(walk, outputs, slist) {
        if (!walk->active)
            continue;
        DLOG("checking output %s\n", walk->name);
        trayclient *trayclient;
        TAILQ_FOREACH(trayclient, walk->trayclients, tailq) {
            if (trayclient->win != event->window)
                continue;

            DLOG("Removing tray client with window ID %08x\n", event->window);
            TAILQ_REMOVE(walk->trayclients, trayclient, tailq);

            /* Trigger an update, we now have more space for the statusline */
            configure_trayclients();
            draw_bars();
            return;
        }
    }
}

/*
 * Handle PropertyNotify messages. Currently only the _XEMBED_INFO property is
 * handled, which tells us whether a dock client should be mapped or unmapped.
 *
 */
static void handle_property_notify(xcb_property_notify_event_t *event) {
    DLOG("PropertyNotify\n");
    if (event->atom == atoms[_XEMBED_INFO] &&
        event->state == XCB_PROPERTY_NEW_VALUE) {
        DLOG("xembed_info updated\n");
        trayclient *trayclient = NULL, *walk;
        i3_output *o_walk;
        SLIST_FOREACH(o_walk, outputs, slist) {
            if (!o_walk->active)
                continue;

            TAILQ_FOREACH(walk, o_walk->trayclients, tailq) {
                if (walk->win != event->window)
                    continue;
                trayclient = walk;
                break;
            }

            if (trayclient)
                break;
        }
        if (!trayclient) {
            ELOG("PropertyNotify received for unknown window %08x\n",
                 event->window);
            return;
        }
        xcb_get_property_cookie_t xembedc;
        xembedc = xcb_get_property_unchecked(xcb_connection,
                                             0,
                                             trayclient->win,
                                             atoms[_XEMBED_INFO],
                                             XCB_GET_PROPERTY_TYPE_ANY,
                                             0,
                                             2 * 32);

        xcb_get_property_reply_t *xembedr = xcb_get_property_reply(xcb_connection,
                                                                   xembedc,
                                                                   NULL);
        if (xembedr == NULL || xembedr->length == 0) {
            DLOG("xembed_info unset\n");
            return;
        }

        DLOG("xembed format = %d, len = %d\n", xembedr->format, xembedr->length);
        uint32_t *xembed = xcb_get_property_value(xembedr);
        DLOG("xembed version = %d\n", xembed[0]);
        DLOG("xembed flags = %d\n", xembed[1]);
        bool map_it = ((xembed[1] & XEMBED_MAPPED) == XEMBED_MAPPED);
        DLOG("map-state now %d\n", map_it);
        if (trayclient->mapped && !map_it) {
            /* need to unmap the window */
            xcb_unmap_window(xcb_connection, trayclient->win);
            trayclient->mapped = map_it;
            configure_trayclients();
            draw_bars();
        } else if (!trayclient->mapped && map_it) {
            /* need to map the window */
            xcb_map_window(xcb_connection, trayclient->win);
            trayclient->mapped = map_it;
            configure_trayclients();
            draw_bars();
        }
        free(xembedr);
    }
}

/*
 * Handle ConfigureRequests by denying them and sending the client a
 * ConfigureNotify with its actual size.
 *
 */
static void handle_configure_request(xcb_configure_request_event_t *event) {
    DLOG("ConfigureRequest for window = %08x\n", event->window);

    trayclient *trayclient;
    i3_output *output;
    SLIST_FOREACH(output, outputs, slist) {
        if (!output->active)
            continue;

        int clients = 0;
        TAILQ_FOREACH_REVERSE(trayclient, output->trayclients, tc_head, tailq) {
            if (!trayclient->mapped)
                continue;
            clients++;

            if (trayclient->win != event->window)
                continue;

            xcb_rectangle_t rect;
            rect.x = output->rect.w - (clients * (font.height + 2));
            rect.y = 2;
            rect.width = font.height;
            rect.height = font.height;

            DLOG("This is a tray window. x = %d\n", rect.x);
            fake_configure_notify(xcb_connection, rect, event->window, 0);
            return;
        }
    }

    DLOG("WARNING: Could not find corresponding tray window.\n");
}

/*
 * This function is called immediately before the main loop locks. We flush xcb
 * then (and only then)
 *
 */
void xcb_prep_cb(struct ev_loop *loop, ev_prepare *watcher, int revents) {
    xcb_flush(xcb_connection);
}

/*
 * This function is called immediately after the main loop locks, so when one
 * of the watchers registered an event.
 * We check whether an X-Event arrived and handle it.
 *
 */
void xcb_chk_cb(struct ev_loop *loop, ev_check *watcher, int revents) {
    xcb_generic_event_t *event;

    if (xcb_connection_has_error(xcb_connection)) {
        ELOG("X11 connection was closed unexpectedly - maybe your X server terminated / crashed?\n");
        exit(1);
    }

    while ((event = xcb_poll_for_event(xcb_connection)) != NULL) {
        switch (event->response_type & ~0x80) {
            case XCB_EXPOSE:
                /* Expose-events happen, when the window needs to be redrawn */
                redraw_bars();
                break;
            case XCB_BUTTON_PRESS:
                /* Button-press-events are mouse-buttons clicked on one of our bars */
                handle_button((xcb_button_press_event_t*) event);
                break;
            case XCB_CLIENT_MESSAGE:
                /* Client messages are used for client-to-client communication, for
                 * example system tray widgets talk to us directly via client messages. */
                handle_client_message((xcb_client_message_event_t*) event);
                break;
            case XCB_UNMAP_NOTIFY:
            case XCB_DESTROY_NOTIFY:
                /* UnmapNotifies are received when a tray window unmaps itself */
                handle_unmap_notify((xcb_unmap_notify_event_t*) event);
                break;
            case XCB_PROPERTY_NOTIFY:
                /* PropertyNotify */
                handle_property_notify((xcb_property_notify_event_t*) event);
                break;
            case XCB_CONFIGURE_REQUEST:
                /* ConfigureRequest, sent by a tray child */
                handle_configure_request((xcb_configure_request_event_t*) event);
                break;
        }
        free(event);
    }
}

/*
 * Dummy Callback. We only need this, so that the Prepare- and Check-Watchers
 * are triggered
 *
 */
void xcb_io_cb(struct ev_loop *loop, ev_io *watcher, int revents) {
}

/*
 * We need to bind to the modifier per XKB. Sadly, XCB does not implement this
 *
 */
void xkb_io_cb(struct ev_loop *loop, ev_io *watcher, int revents) {
    XkbEvent ev;
    int modstate = 0;

    DLOG("Got XKB-Event!\n");

    while (XPending(xkb_dpy)) {
        XNextEvent(xkb_dpy, (XEvent*)&ev);

        if (ev.type != xkb_event_base) {
            ELOG("No Xkb-Event!\n");
            continue;
        }

        if (ev.any.xkb_type != XkbStateNotify) {
            ELOG("No State Notify!\n");
            continue;
        }

        unsigned int mods = ev.state.mods;
        modstate = mods & config.modifier;
    }

#define DLOGMOD(modmask, status, barfunc) \
    do { \
        switch (modmask) { \
            case ShiftMask: \
                DLOG("ShiftMask got " #status "!\n"); \
                break; \
            case ControlMask: \
                DLOG("ControlMask got " #status "!\n"); \
                break; \
            case Mod1Mask: \
                DLOG("Mod1Mask got " #status "!\n"); \
                break; \
            case Mod2Mask: \
                DLOG("Mod2Mask got " #status "!\n"); \
                break; \
            case Mod3Mask: \
                DLOG("Mod3Mask got " #status "!\n"); \
                break; \
            case Mod4Mask: \
                DLOG("Mod4Mask got " #status "!\n"); \
                break; \
            case Mod5Mask: \
                DLOG("Mod5Mask got " #status "!\n"); \
                break; \
        } \
        barfunc(); \
    } while (0)

    if (modstate != mod_pressed) {
        if (modstate == 0) {
            DLOGMOD(config.modifier, released, hide_bars);
        } else {
            DLOGMOD(config.modifier, pressed, unhide_bars);
        }
        mod_pressed = modstate;
    }

#undef DLOGMOD
}

/*
 * Early initialization of the connection to X11: Everything which does not
 * depend on 'config'.
 *
 */
char *init_xcb_early() {
    /* FIXME: xcb_connect leaks Memory */
    xcb_connection = xcb_connect(NULL, &screen);
    if (xcb_connection_has_error(xcb_connection)) {
        ELOG("Cannot open display\n");
        exit(EXIT_FAILURE);
    }
    conn = xcb_connection;
    DLOG("Connected to xcb\n");

    /* We have to request the atoms we need */
    #define ATOM_DO(name) atom_cookies[name] = xcb_intern_atom(xcb_connection, 0, strlen(#name), #name);
    #include "xcb_atoms.def"

    root_screen = xcb_aux_get_screen(xcb_connection, screen);
    xcb_root = root_screen->root;

    /* We draw the statusline to a seperate pixmap, because it looks the same on all bars and
     * this way, we can choose to crop it */
    uint32_t mask = XCB_GC_FOREGROUND;
    uint32_t vals[] = { colors.bar_bg, colors.bar_bg };

    statusline_clear = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t clear_ctx_cookie = xcb_create_gc_checked(xcb_connection,
                                                               statusline_clear,
                                                               xcb_root,
                                                               mask,
                                                               vals);

    statusline_ctx = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t sl_ctx_cookie = xcb_create_gc_checked(xcb_connection,
                                                            statusline_ctx,
                                                            xcb_root,
                                                            0,
                                                            NULL);

    statusline_pm = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t sl_pm_cookie = xcb_create_pixmap_checked(xcb_connection,
                                                               root_screen->root_depth,
                                                               statusline_pm,
                                                               xcb_root,
                                                               root_screen->width_in_pixels,
                                                               root_screen->height_in_pixels);


    /* The various Watchers to communicate with xcb */
    xcb_io = smalloc(sizeof(ev_io));
    xcb_prep = smalloc(sizeof(ev_prepare));
    xcb_chk = smalloc(sizeof(ev_check));

    ev_io_init(xcb_io, &xcb_io_cb, xcb_get_file_descriptor(xcb_connection), EV_READ);
    ev_prepare_init(xcb_prep, &xcb_prep_cb);
    ev_check_init(xcb_chk, &xcb_chk_cb);

    ev_io_start(main_loop, xcb_io);
    ev_prepare_start(main_loop, xcb_prep);
    ev_check_start(main_loop, xcb_chk);

    /* Now we get the atoms and save them in a nice data structure */
    get_atoms();

    xcb_get_property_cookie_t path_cookie;
    path_cookie = xcb_get_property_unchecked(xcb_connection,
                                   0,
                                   xcb_root,
                                   atoms[I3_SOCKET_PATH],
                                   XCB_GET_PROPERTY_TYPE_ANY,
                                   0, PATH_MAX);

    /* We check, if i3 set its socket-path */
    xcb_get_property_reply_t *path_reply = xcb_get_property_reply(xcb_connection,
                                                                  path_cookie,
                                                                  NULL);
    char *path = NULL;
    if (path_reply) {
        int len = xcb_get_property_value_length(path_reply);
        if (len != 0) {
            path = strndup(xcb_get_property_value(path_reply), len);
        }
    }


    if (xcb_request_failed(sl_pm_cookie, "Could not allocate statusline-buffer") ||
        xcb_request_failed(clear_ctx_cookie, "Could not allocate statusline-buffer-clearcontext") ||
        xcb_request_failed(sl_ctx_cookie, "Could not allocate statusline-buffer-context")) {
        exit(EXIT_FAILURE);
    }

    return path;
}

/*
 * Initialization which depends on 'config' being usable. Called after the
 * configuration has arrived.
 *
 */
void init_xcb_late(char *fontname) {
    if (fontname == NULL)
        fontname = "-misc-fixed-medium-r-normal--13-120-75-75-C-70-iso10646-1";

    /* Load the font */
    font = load_font(fontname, true);
    set_font(&font);
    DLOG("Calculated Font-height: %d\n", font.height);

    xcb_flush(xcb_connection);

    /* To grab modifiers without blocking other applications from receiving key-events
     * involving that modifier, we sadly have to use xkb which is not yet fully supported
     * in xcb */
    if (config.hide_on_modifier) {
        int xkb_major, xkb_minor, xkb_errbase, xkb_err;
        xkb_major = XkbMajorVersion;
        xkb_minor = XkbMinorVersion;

        xkb_dpy = XkbOpenDisplay(NULL,
                                 &xkb_event_base,
                                 &xkb_errbase,
                                 &xkb_major,
                                 &xkb_minor,
                                 &xkb_err);

        if (xkb_dpy == NULL) {
            ELOG("No XKB!\n");
            exit(EXIT_FAILURE);
        }

        if (fcntl(ConnectionNumber(xkb_dpy), F_SETFD, FD_CLOEXEC) == -1) {
            ELOG("Could not set FD_CLOEXEC on xkbdpy: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        int i1;
        if (!XkbQueryExtension(xkb_dpy, &i1, &xkb_event_base, &xkb_errbase, &xkb_major, &xkb_minor)) {
            ELOG("XKB not supported by X-server!\n");
            exit(EXIT_FAILURE);
        }

        if (!XkbSelectEvents(xkb_dpy, XkbUseCoreKbd, XkbStateNotifyMask, XkbStateNotifyMask)) {
            ELOG("Could not grab Key!\n");
            exit(EXIT_FAILURE);
        }

        xkb_io = smalloc(sizeof(ev_io));
        ev_io_init(xkb_io, &xkb_io_cb, ConnectionNumber(xkb_dpy), EV_READ);
        ev_io_start(main_loop, xkb_io);
        XFlush(xkb_dpy);
    }
}

/*
 * Initializes tray support by requesting the appropriate _NET_SYSTEM_TRAY atom
 * for the X11 display we are running on, then acquiring the selection for this
 * atom. Afterwards, tray clients will send ClientMessages to our window.
 *
 */
void init_tray(void) {
    DLOG("Initializing system tray functionality\n");
    /* request the tray manager atom for the X11 display we are running on */
    char atomname[strlen("_NET_SYSTEM_TRAY_S") + 11];
    snprintf(atomname, strlen("_NET_SYSTEM_TRAY_S") + 11, "_NET_SYSTEM_TRAY_S%d", screen);
    xcb_intern_atom_cookie_t tray_cookie;
    xcb_intern_atom_reply_t *tray_reply;
    tray_cookie = xcb_intern_atom(xcb_connection, 0, strlen(atomname), atomname);

    /* tray support: we need a window to own the selection */
    xcb_window_t selwin = xcb_generate_id(xcb_connection);
    uint32_t selmask = XCB_CW_OVERRIDE_REDIRECT;
    uint32_t selval[] = { 1 };
    xcb_create_window(xcb_connection,
                      root_screen->root_depth,
                      selwin,
                      xcb_root,
                      -1, -1,
                      1, 1,
                      1,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      root_screen->root_visual,
                      selmask,
                      selval);

    uint32_t orientation = _NET_SYSTEM_TRAY_ORIENTATION_HORZ;
    /* set the atoms */
    xcb_change_property(xcb_connection,
                        XCB_PROP_MODE_REPLACE,
                        selwin,
                        atoms[_NET_SYSTEM_TRAY_ORIENTATION],
                        XCB_ATOM_CARDINAL,
                        32,
                        1,
                        &orientation);

    if (!(tray_reply = xcb_intern_atom_reply(xcb_connection, tray_cookie, NULL))) {
        ELOG("Could not get atom %s\n", atomname);
        exit(EXIT_FAILURE);
    }

    xcb_set_selection_owner(xcb_connection,
                            selwin,
                            tray_reply->atom,
                            XCB_CURRENT_TIME);

    /* Verify that we have the selection */
    xcb_get_selection_owner_cookie_t selcookie;
    xcb_get_selection_owner_reply_t *selreply;

    selcookie = xcb_get_selection_owner(xcb_connection, tray_reply->atom);
    if (!(selreply = xcb_get_selection_owner_reply(xcb_connection, selcookie, NULL))) {
        ELOG("Could not get selection owner for %s\n", atomname);
        exit(EXIT_FAILURE);
    }

    if (selreply->owner != selwin) {
        ELOG("Could not set the %s selection. " \
             "Maybe another tray is already running?\n", atomname);
        /* NOTE that this error is not fatal. We just can’t provide tray
         * functionality */
        free(selreply);
        return;
    }

    /* Inform clients waiting for a new _NET_SYSTEM_TRAY that we are here */
    void *event = scalloc(32);
    xcb_client_message_event_t *ev = event;
    ev->response_type = XCB_CLIENT_MESSAGE;
    ev->window = xcb_root;
    ev->type = atoms[MANAGER];
    ev->format = 32;
    ev->data.data32[0] = XCB_CURRENT_TIME;
    ev->data.data32[1] = tray_reply->atom;
    ev->data.data32[2] = selwin;
    xcb_send_event(xcb_connection,
                   0,
                   xcb_root,
                   0xFFFFFF,
                   (char*)ev);
    free(event);
    free(tray_reply);
}

/*
 * Cleanup the xcb-stuff.
 * Called once, before the program terminates.
 *
 */
void clean_xcb(void) {
    i3_output *o_walk;
    free_workspaces();
    SLIST_FOREACH(o_walk, outputs, slist) {
        destroy_window(o_walk);
        FREE(o_walk->trayclients);
        FREE(o_walk->workspaces);
        FREE(o_walk->name);
    }
    FREE_SLIST(outputs, i3_output);
    FREE(outputs);

    xcb_flush(xcb_connection);
    xcb_disconnect(xcb_connection);

    ev_check_stop(main_loop, xcb_chk);
    ev_prepare_stop(main_loop, xcb_prep);
    ev_io_stop(main_loop, xcb_io);

    FREE(xcb_chk);
    FREE(xcb_prep);
    FREE(xcb_io);
}

/*
 * Get the earlier requested atoms and save them in the prepared data structure
 *
 */
void get_atoms(void) {
    xcb_intern_atom_reply_t *reply;
    #define ATOM_DO(name) reply = xcb_intern_atom_reply(xcb_connection, atom_cookies[name], NULL); \
        if (reply == NULL) { \
            ELOG("Could not get atom %s\n", #name); \
            exit(EXIT_FAILURE); \
        } \
        atoms[name] = reply->atom; \
        free(reply);

    #include "xcb_atoms.def"
    DLOG("Got Atoms\n");
}

/*
 * Reparents all tray clients of the specified output to the root window. This
 * is either used when shutting down, when an output appears (xrandr --output
 * VGA1 --off) or when the primary output changes.
 *
 * Applications using the tray will start the protocol from the beginning again
 * afterwards.
 *
 */
void kick_tray_clients(i3_output *output) {
    trayclient *trayclient;
    while (!TAILQ_EMPTY(output->trayclients)) {
        trayclient = TAILQ_FIRST(output->trayclients);
        /* Unmap, then reparent (to root) the tray client windows */
        xcb_unmap_window(xcb_connection, trayclient->win);
        xcb_reparent_window(xcb_connection,
                            trayclient->win,
                            xcb_root,
                            0,
                            0);

        /* We remove the trayclient right here. We might receive an UnmapNotify
         * event afterwards, but better safe than sorry. */
        TAILQ_REMOVE(output->trayclients, trayclient, tailq);
    }
}

/*
 * Destroy the bar of the specified output
 *
 */
void destroy_window(i3_output *output) {
    if (output == NULL) {
        return;
    }
    if (output->bar == XCB_NONE) {
        return;
    }

    kick_tray_clients(output);
    xcb_destroy_window(xcb_connection, output->bar);
    output->bar = XCB_NONE;
}

/*
 * Reallocate the statusline-buffer
 *
 */
void realloc_sl_buffer(void) {
    DLOG("Re-allocating statusline-buffer, statusline_width = %d, root_screen->width_in_pixels = %d\n",
         statusline_width, root_screen->width_in_pixels);
    xcb_free_pixmap(xcb_connection, statusline_pm);
    statusline_pm = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t sl_pm_cookie = xcb_create_pixmap_checked(xcb_connection,
                                                               root_screen->root_depth,
                                                               statusline_pm,
                                                               xcb_root,
                                                               MAX(root_screen->width_in_pixels, statusline_width),
                                                               root_screen->height_in_pixels);

    uint32_t mask = XCB_GC_FOREGROUND;
    uint32_t vals[2] = { colors.bar_bg, colors.bar_bg };
    xcb_free_gc(xcb_connection, statusline_clear);
    statusline_clear = xcb_generate_id(xcb_connection);
    xcb_void_cookie_t clear_ctx_cookie = xcb_create_gc_checked(xcb_connection,
                                                               statusline_clear,
                                                               xcb_root,
                                                               mask,
                                                               vals);

    mask |= XCB_GC_BACKGROUND;
    vals[0] = colors.bar_fg;
    statusline_ctx = xcb_generate_id(xcb_connection);
    xcb_free_gc(xcb_connection, statusline_ctx);
    xcb_void_cookie_t sl_ctx_cookie = xcb_create_gc_checked(xcb_connection,
                                                            statusline_ctx,
                                                            xcb_root,
                                                            mask,
                                                            vals);

    if (xcb_request_failed(sl_pm_cookie, "Could not allocate statusline-buffer") ||
        xcb_request_failed(clear_ctx_cookie, "Could not allocate statusline-buffer-clearcontext") ||
        xcb_request_failed(sl_ctx_cookie, "Could not allocate statusline-buffer-context")) {
        exit(EXIT_FAILURE);
    }

}

/*
 * Reconfigure all bars and create new bars for recently activated outputs
 *
 */
void reconfig_windows(void) {
    uint32_t mask;
    uint32_t values[5];
    static bool tray_configured = false;

    i3_output *walk;
    SLIST_FOREACH(walk, outputs, slist) {
        if (!walk->active) {
            /* If an output is not active, we destroy its bar */
            /* FIXME: Maybe we rather want to unmap? */
            DLOG("Destroying window for output %s\n", walk->name);
            destroy_window(walk);
            continue;
        }
        if (walk->bar == XCB_NONE) {
            DLOG("Creating Window for output %s\n", walk->name);

            walk->bar = xcb_generate_id(xcb_connection);
            walk->buffer = xcb_generate_id(xcb_connection);
            mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
            /* Black background */
            values[0] = colors.bar_bg;
            /* If hide_on_modifier is set, i3 is not supposed to manage our bar-windows */
            values[1] = config.hide_on_modifier;
            /* We enable the following EventMask fields:
             * EXPOSURE, to get expose events (we have to re-draw then)
             * SUBSTRUCTURE_REDIRECT, to get ConfigureRequests when the tray
             *                        child windows use ConfigureWindow
             * BUTTON_PRESS, to handle clicks on the workspace buttons
             * */
            values[2] = XCB_EVENT_MASK_EXPOSURE |
                        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT;
            if (!config.disable_ws) {
                values[2] |= XCB_EVENT_MASK_BUTTON_PRESS;
            }
            xcb_void_cookie_t win_cookie = xcb_create_window_checked(xcb_connection,
                                                                     root_screen->root_depth,
                                                                     walk->bar,
                                                                     xcb_root,
                                                                     walk->rect.x, walk->rect.y + walk->rect.h - font.height - 6,
                                                                     walk->rect.w, font.height + 6,
                                                                     1,
                                                                     XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                                                     root_screen->root_visual,
                                                                     mask,
                                                                     values);

            /* The double-buffer we use to render stuff off-screen */
            xcb_void_cookie_t pm_cookie = xcb_create_pixmap_checked(xcb_connection,
                                                                    root_screen->root_depth,
                                                                    walk->buffer,
                                                                    walk->bar,
                                                                    walk->rect.w,
                                                                    walk->rect.h);

            /* Set the WM_CLASS and WM_NAME (we don't need UTF-8) atoms */
            xcb_void_cookie_t class_cookie;
            class_cookie = xcb_change_property(xcb_connection,
                                               XCB_PROP_MODE_REPLACE,
                                               walk->bar,
                                               XCB_ATOM_WM_CLASS,
                                               XCB_ATOM_STRING,
                                               8,
                                               (strlen("i3bar") + 1) * 2,
                                               "i3bar\0i3bar\0");

            char *name;
            if (asprintf(&name, "i3bar for output %s", walk->name) == -1)
                err(EXIT_FAILURE, "asprintf()");
            xcb_void_cookie_t name_cookie;
            name_cookie = xcb_change_property(xcb_connection,
                                              XCB_PROP_MODE_REPLACE,
                                              walk->bar,
                                              XCB_ATOM_WM_NAME,
                                              XCB_ATOM_STRING,
                                              8,
                                              strlen(name),
                                              name);
            free(name);

            /* We want dock-windows (for now). When override_redirect is set, i3 is ignoring
             * this one */
            xcb_void_cookie_t dock_cookie = xcb_change_property(xcb_connection,
                                                                XCB_PROP_MODE_REPLACE,
                                                                walk->bar,
                                                                atoms[_NET_WM_WINDOW_TYPE],
                                                                XCB_ATOM_ATOM,
                                                                32,
                                                                1,
                                                                (unsigned char*) &atoms[_NET_WM_WINDOW_TYPE_DOCK]);

            /* We need to tell i3, where to reserve space for i3bar */
            /* left, right, top, bottom, left_start_y, left_end_y,
             * right_start_y, right_end_y, top_start_x, top_end_x, bottom_start_x,
             * bottom_end_x */
            /* A local struct to save the strut_partial property */
            struct {
                uint32_t left;
                uint32_t right;
                uint32_t top;
                uint32_t bottom;
                uint32_t left_start_y;
                uint32_t left_end_y;
                uint32_t right_start_y;
                uint32_t right_end_y;
                uint32_t top_start_x;
                uint32_t top_end_x;
                uint32_t bottom_start_x;
                uint32_t bottom_end_x;
            } __attribute__((__packed__)) strut_partial = {0,};
            switch (config.position) {
                case POS_NONE:
                    break;
                case POS_TOP:
                    strut_partial.top = font.height + 6;
                    strut_partial.top_start_x = walk->rect.x;
                    strut_partial.top_end_x = walk->rect.x + walk->rect.w;
                    break;
                case POS_BOT:
                    strut_partial.bottom = font.height + 6;
                    strut_partial.bottom_start_x = walk->rect.x;
                    strut_partial.bottom_end_x = walk->rect.x + walk->rect.w;
                    break;
            }
            xcb_void_cookie_t strut_cookie = xcb_change_property(xcb_connection,
                                                                 XCB_PROP_MODE_REPLACE,
                                                                 walk->bar,
                                                                 atoms[_NET_WM_STRUT_PARTIAL],
                                                                 XCB_ATOM_CARDINAL,
                                                                 32,
                                                                 12,
                                                                 &strut_partial);

            /* We also want a graphics-context for the bars (it defines the properties
             * with which we draw to them) */
            walk->bargc = xcb_generate_id(xcb_connection);
            xcb_void_cookie_t gc_cookie = xcb_create_gc_checked(xcb_connection,
                                                                walk->bargc,
                                                                walk->bar,
                                                                0,
                                                                NULL);

            /* We finally map the bar (display it on screen), unless the modifier-switch is on */
            xcb_void_cookie_t map_cookie;
            if (!config.hide_on_modifier) {
                map_cookie = xcb_map_window_checked(xcb_connection, walk->bar);
            }

            if (xcb_request_failed(win_cookie,   "Could not create window") ||
                xcb_request_failed(pm_cookie,    "Could not create pixmap") ||
                xcb_request_failed(dock_cookie,  "Could not set dock mode") ||
                xcb_request_failed(class_cookie, "Could not set WM_CLASS")  ||
                xcb_request_failed(name_cookie,  "Could not set WM_NAME")   ||
                xcb_request_failed(strut_cookie, "Could not set strut")     ||
                xcb_request_failed(gc_cookie,    "Could not create graphical context") ||
                (!config.hide_on_modifier && xcb_request_failed(map_cookie, "Could not map window"))) {
                exit(EXIT_FAILURE);
            }

            if (!tray_configured &&
                (!config.tray_output ||
                 strcasecmp("none", config.tray_output) != 0)) {
                init_tray();
                tray_configured = true;
            }
        } else {
            /* We already have a bar, so we just reconfigure it */
            mask = XCB_CONFIG_WINDOW_X |
                   XCB_CONFIG_WINDOW_Y |
                   XCB_CONFIG_WINDOW_WIDTH |
                   XCB_CONFIG_WINDOW_HEIGHT |
                   XCB_CONFIG_WINDOW_STACK_MODE;
            values[0] = walk->rect.x;
            values[1] = walk->rect.y + walk->rect.h - font.height - 6;
            values[2] = walk->rect.w;
            values[3] = font.height + 6;
            values[4] = XCB_STACK_MODE_ABOVE;

            DLOG("Destroying buffer for output %s", walk->name);
            xcb_free_pixmap(xcb_connection, walk->buffer);

            DLOG("Reconfiguring Window for output %s to %d,%d\n", walk->name, values[0], values[1]);
            xcb_void_cookie_t cfg_cookie = xcb_configure_window_checked(xcb_connection,
                                                                        walk->bar,
                                                                        mask,
                                                                        values);

            DLOG("Recreating buffer for output %s", walk->name);
            xcb_void_cookie_t pm_cookie = xcb_create_pixmap_checked(xcb_connection,
                                                                    root_screen->root_depth,
                                                                    walk->buffer,
                                                                    walk->bar,
                                                                    walk->rect.w,
                                                                    walk->rect.h);

            if (xcb_request_failed(cfg_cookie, "Could not reconfigure window")) {
                exit(EXIT_FAILURE);
            }
            if (xcb_request_failed(pm_cookie,  "Could not create pixmap")) {
                exit(EXIT_FAILURE);
            }
        }
    }
}

/*
 * Render the bars, with buttons and statusline
 *
 */
void draw_bars(void) {
    DLOG("Drawing Bars...\n");
    int i = 0;

    refresh_statusline();

    i3_output *outputs_walk;
    SLIST_FOREACH(outputs_walk, outputs, slist) {
        if (!outputs_walk->active) {
            DLOG("Output %s inactive, skipping...\n", outputs_walk->name);
            continue;
        }
        if (outputs_walk->bar == XCB_NONE) {
            /* Oh shit, an active output without an own bar. Create it now! */
            reconfig_windows();
        }
        /* First things first: clear the backbuffer */
        uint32_t color = colors.bar_bg;
        xcb_change_gc(xcb_connection,
                      outputs_walk->bargc,
                      XCB_GC_FOREGROUND,
                      &color);
        xcb_rectangle_t rect = { 0, 0, outputs_walk->rect.w, font.height + 6 };
        xcb_poly_fill_rectangle(xcb_connection,
                                outputs_walk->buffer,
                                outputs_walk->bargc,
                                1,
                                &rect);

        if (!TAILQ_EMPTY(&statusline_head)) {
            DLOG("Printing statusline!\n");

            /* Luckily we already prepared a seperate pixmap containing the rendered
             * statusline, we just have to copy the relevant parts to the relevant
             * position */
            trayclient *trayclient;
            int traypx = 0;
            TAILQ_FOREACH(trayclient, outputs_walk->trayclients, tailq) {
                if (!trayclient->mapped)
                    continue;
                /* We assume the tray icons are quadratic (we use the font
                 * *height* as *width* of the icons) because we configured them
                 * like this. */
                traypx += font.height + 2;
            }
            /* Add 2px of padding if there are any tray icons */
            if (traypx > 0)
                traypx += 2;
            xcb_copy_area(xcb_connection,
                          statusline_pm,
                          outputs_walk->buffer,
                          outputs_walk->bargc,
                          MAX(0, (int16_t)(statusline_width - outputs_walk->rect.w + 4)), 0,
                          MAX(0, (int16_t)(outputs_walk->rect.w - statusline_width - traypx - 4)), 3,
                          MIN(outputs_walk->rect.w - traypx - 4, statusline_width), font.height);
        }

        if (config.disable_ws) {
            continue;
        }

        i3_ws *ws_walk;
        static char *last_urgent_ws = NULL;
        bool has_urgent = false, walks_away = true;

        TAILQ_FOREACH(ws_walk, outputs_walk->workspaces, tailq) {
            DLOG("Drawing Button for WS %s at x = %d, len = %d\n", i3string_as_utf8(ws_walk->name), i, ws_walk->name_width);
            uint32_t fg_color = colors.inactive_ws_fg;
            uint32_t bg_color = colors.inactive_ws_bg;
            uint32_t border_color = colors.inactive_ws_border;
            if (ws_walk->visible) {
                if (!ws_walk->focused) {
                    fg_color = colors.active_ws_fg;
                    bg_color = colors.active_ws_bg;
                    border_color = colors.active_ws_border;
                } else {
                    fg_color = colors.focus_ws_fg;
                    bg_color = colors.focus_ws_bg;
                    border_color = colors.focus_ws_border;
                    if (last_urgent_ws && strcmp(i3string_as_utf8(ws_walk->name), last_urgent_ws) == 0)
                        walks_away = false;
                }
            }
            if (ws_walk->urgent) {
                DLOG("WS %s is urgent!\n", i3string_as_utf8(ws_walk->name));
                fg_color = colors.urgent_ws_fg;
                bg_color = colors.urgent_ws_bg;
                border_color = colors.urgent_ws_border;
                has_urgent = true;
                if (!ws_walk->focused) {
                    FREE(last_urgent_ws);
                    last_urgent_ws = sstrdup(i3string_as_utf8(ws_walk->name));
                }
                /* The urgent-hint should get noticed, so we unhide the bars shortly */
                unhide_bars();
            }
            uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;
            uint32_t vals_border[] = { border_color, border_color };
            xcb_change_gc(xcb_connection,
                          outputs_walk->bargc,
                          mask,
                          vals_border);
            xcb_rectangle_t rect_border = { i, 0, ws_walk->name_width + 10, font.height + 4 };
            xcb_poly_fill_rectangle(xcb_connection,
                                    outputs_walk->buffer,
                                    outputs_walk->bargc,
                                    1,
                                    &rect_border);
            uint32_t vals[] = { bg_color, bg_color };
            xcb_change_gc(xcb_connection,
                          outputs_walk->bargc,
                          mask,
                          vals);
            xcb_rectangle_t rect = { i + 1, 1, ws_walk->name_width + 8, font.height + 2 };
            xcb_poly_fill_rectangle(xcb_connection,
                                    outputs_walk->buffer,
                                    outputs_walk->bargc,
                                    1,
                                    &rect);
            set_font_colors(outputs_walk->bargc, fg_color, bg_color);
            draw_text(ws_walk->name, outputs_walk->buffer, outputs_walk->bargc, i + 5, 2, ws_walk->name_width);
            i += 10 + ws_walk->name_width + 1;
        }

        if (!has_urgent && !mod_pressed && walks_away) {
            FREE(last_urgent_ws);
            hide_bars();
        }

        i = 0;
    }

    redraw_bars();
}

/*
 * Redraw the bars, i.e. simply copy the buffer to the barwindow
 *
 */
void redraw_bars(void) {
    i3_output *outputs_walk;
    SLIST_FOREACH(outputs_walk, outputs, slist) {
        if (!outputs_walk->active) {
            continue;
        }
        xcb_copy_area(xcb_connection,
                      outputs_walk->buffer,
                      outputs_walk->bar,
                      outputs_walk->bargc,
                      0, 0,
                      0, 0,
                      outputs_walk->rect.w,
                      outputs_walk->rect.h);
        xcb_flush(xcb_connection);
    }
}
