/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2010 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include <time.h>

#include <xcb/xcb_atom.h>
#include <xcb/randr.h>

#include <X11/XKBlib.h>

#include "all.h"

/* After mapping/unmapping windows, a notify event is generated. However, we don’t want it,
   since it’d trigger an infinite loop of switching between the different windows when
   changing workspaces */
static SLIST_HEAD(ignore_head, Ignore_Event) ignore_events;

void add_ignore_event(const int sequence) {
    struct Ignore_Event *event = smalloc(sizeof(struct Ignore_Event));

    event->sequence = sequence;
    event->added = time(NULL);

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
        if (event->sequence != sequence)
            continue;

        /* instead of removing a sequence number we better wait until it gets
         * garbage collected. it may generate multiple events (there are multiple
         * enter_notifies for one configure_request, for example). */
        //SLIST_REMOVE(&ignore_events, event, Ignore_Event, ignore_events);
        //free(event);
        return true;
    }

    return false;
}

/*
 * There was a key press. We compare this key code with our bindings table and pass
 * the bound action to parse_command().
 *
 */
int handle_key_press(void *ignored, xcb_connection_t *conn, xcb_key_press_event_t *event) {
    DLOG("Keypress %d, state raw = %d\n", event->detail, event->state);

    /* Remove the numlock bit, all other bits are modifiers we can bind to */
    uint16_t state_filtered = event->state & ~(xcb_numlock_mask | XCB_MOD_MASK_LOCK);
    DLOG("(removed numlock, state = %d)\n", state_filtered);
    /* Only use the lower 8 bits of the state (modifier masks) so that mouse
     * button masks are filtered out */
    state_filtered &= 0xFF;
    DLOG("(removed upper 8 bits, state = %d)\n", state_filtered);

    if (xkb_current_group == XkbGroup2Index)
        state_filtered |= BIND_MODE_SWITCH;

    DLOG("(checked mode_switch, state %d)\n", state_filtered);

    /* Find the binding */
    Binding *bind = get_binding(state_filtered, event->detail);

    /* No match? Then the user has Mode_switch enabled but does not have a
     * specific keybinding. Fall back to the default keybindings (without
     * Mode_switch). Makes it much more convenient for users of a hybrid
     * layout (like us, ru). */
    if (bind == NULL) {
        state_filtered &= ~(BIND_MODE_SWITCH);
        DLOG("no match, new state_filtered = %d\n", state_filtered);
        if ((bind = get_binding(state_filtered, event->detail)) == NULL) {
            ELOG("Could not lookup key binding (modifiers %d, keycode %d)\n",
                 state_filtered, event->detail);
            return 1;
        }
    }

    parse_command(bind->command);
    return 1;
}

#if 0

/*
 * Called with coordinates of an enter_notify event or motion_notify event
 * to check if the user crossed virtual screen boundaries and adjust the
 * current workspace, if so.
 *
 */
static void check_crossing_screen_boundary(uint32_t x, uint32_t y) {
        Output *output;

        if ((output = get_output_containing(x, y)) == NULL) {
                ELOG("ERROR: No such screen\n");
                return;
        }
        if (output == c_ws->output)
                return;

        c_ws->current_row = current_row;
        c_ws->current_col = current_col;
        c_ws = output->current_workspace;
        current_row = c_ws->current_row;
        current_col = c_ws->current_col;
        DLOG("We're now on output %p\n", output);

        /* While usually this function is only called when the user switches
         * to a different output using his mouse (and thus the output is
         * empty), it may be that the following race condition occurs:
         * 1) the user actives a new output (say VGA1).
         * 2) the cursor is sent to the first pixel of the new VGA1, thus
         *    generating an enter_notify for the screen (the enter_notify
         *    is not yet received by i3).
         * 3) i3 requeries screen configuration and maps a workspace onto the
         *    new output.
         * 4) the enter_notify event arrives and c_ws is set to the new
         *    workspace but the existing windows on the new workspace are not
         *    focused.
         *
         * Therefore, we re-set the focus here to be sure it’s correct. */
        Client *first_client = SLIST_FIRST(&(c_ws->focus_stack));
        if (first_client != NULL)
                set_focus(global_conn, first_client, true);
}
#endif

/*
 * When the user moves the mouse pointer onto a window, this callback gets called.
 *
 */
int handle_enter_notify(void *ignored, xcb_connection_t *conn,
                        xcb_enter_notify_event_t *event) {
    Con *con;

    DLOG("enter_notify for %08x, mode = %d, detail %d, serial %d\n",
         event->event, event->mode, event->detail, event->sequence);
    DLOG("coordinates %x, %x\n", event->event_x, event->event_y);
    if (event->mode != XCB_NOTIFY_MODE_NORMAL) {
        DLOG("This was not a normal notify, ignoring\n");
        return 1;
    }
    /* Some events are not interesting, because they were not generated
     * actively by the user, but by reconfiguration of windows */
    if (event_is_ignored(event->sequence))
        return 1;

    /* Get container by frame or by child window */
    if ((con = con_by_frame_id(event->event)) == NULL)
        con = con_by_window_id(event->event);

    /* If not, then the user moved his cursor to the root window. In that case, we adjust c_ws */
    if (con == NULL) {
        DLOG("Getting screen at %d x %d\n", event->root_x, event->root_y);
        //check_crossing_screen_boundary(event->root_x, event->root_y);
        return 1;
    }

    /* see if the user entered the window on a certain window decoration */
    int layout = con->layout;
    Con *child;
    TAILQ_FOREACH(child, &(con->nodes_head), nodes)
        if (rect_contains(child->deco_rect, event->event_x, event->event_y)) {
            LOG("using child %p / %s instead!\n", child, child->name);
            con = child;
            break;
        }

    /* for stacked/tabbed layout we do not want to change focus when the user
     * enters the window at the decoration of any child window. */
    if (layout == L_STACKED || layout == L_TABBED) {
        con = TAILQ_FIRST(&(con->parent->focus_head));
        LOG("using focused %p / %s instead\n", con, con->name);
    }

#if 0
    if (client->workspace != c_ws && client->workspace->output == c_ws->output) {
            /* This can happen when a client gets assigned to a different workspace than
             * the current one (see src/mainx.c:reparent_window). Shortly after it was created,
             * an enter_notify will follow. */
            DLOG("enter_notify for a client on a different workspace but the same screen, ignoring\n");
            return 1;
    }
#endif

    if (config.disable_focus_follows_mouse)
        return 1;
    Con *next = con;
    while (!TAILQ_EMPTY(&(next->focus_head)))
        next = TAILQ_FIRST(&(next->focus_head));

    con_focus(next);
    x_push_changes(croot);

    return 1;
}
#if 0

/*
 * When the user moves the mouse but does not change the active window
 * (e.g. when having no windows opened but moving mouse on the root screen
 * and crossing virtual screen boundaries), this callback gets called.
 *
 */
int handle_motion_notify(void *ignored, xcb_connection_t *conn, xcb_motion_notify_event_t *event) {
        /* Skip events where the pointer was over a child window, we are only
         * interested in events on the root window. */
        if (event->child != 0)
                return 1;

        check_crossing_screen_boundary(event->root_x, event->root_y);

        return 1;
}

/*
 * Called when the keyboard mapping changes (for example by using Xmodmap),
 * we need to update our key bindings then (re-translate symbols).
 *
 */
int handle_mapping_notify(void *ignored, xcb_connection_t *conn, xcb_mapping_notify_event_t *event) {
        if (event->request != XCB_MAPPING_KEYBOARD &&
            event->request != XCB_MAPPING_MODIFIER)
                return 0;

        DLOG("Received mapping_notify for keyboard or modifier mapping, re-grabbing keys\n");
        xcb_refresh_keyboard_mapping(keysyms, event);

        xcb_get_numlock_mask(conn);

        ungrab_all_keys(conn);
        translate_keysyms();
        grab_all_keys(conn, false);

        return 0;
}

#endif
/*
 * A new window appeared on the screen (=was mapped), so let’s manage it.
 *
 */
int handle_map_request(void *prophs, xcb_connection_t *conn, xcb_map_request_event_t *event) {
    xcb_get_window_attributes_cookie_t cookie;

    cookie = xcb_get_window_attributes_unchecked(conn, event->window);

    DLOG("window = 0x%08x, serial is %d.\n", event->window, event->sequence);
    add_ignore_event(event->sequence);

    manage_window(event->window, cookie, false);
    return 1;
}
#if 0
/*
 * Configure requests are received when the application wants to resize windows on their own.
 *
 * We generate a synthethic configure notify event to signalize the client its "new" position.
 *
 */
int handle_configure_request(void *prophs, xcb_connection_t *conn, xcb_configure_request_event_t *event) {
        DLOG("window 0x%08x wants to be at %dx%d with %dx%d\n",
            event->window, event->x, event->y, event->width, event->height);

        Client *client = table_get(&by_child, event->window);
        if (client == NULL) {
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

        if (client->fullscreen) {
                DLOG("Client is in fullscreen mode\n");

                Rect child_rect = client->workspace->rect;
                child_rect.x = child_rect.y = 0;
                fake_configure_notify(conn, child_rect, client->child);

                return 1;
        }

        /* Floating clients can be reconfigured */
        if (client_is_floating(client)) {
                i3Font *font = load_font(conn, config.font);
                int mode = (client->container != NULL ? client->container->mode : MODE_DEFAULT);
                /* TODO: refactor this code. we need a function to translate
                 * coordinates of child_rect/rect. */

                if (event->value_mask & XCB_CONFIG_WINDOW_X) {
                        if (mode == MODE_STACK || mode == MODE_TABBED) {
                                client->rect.x = event->x - 2;
                        } else {
                                if (client->titlebar_position == TITLEBAR_OFF && client->borderless)
                                        client->rect.x = event->x;
                                else if (client->titlebar_position == TITLEBAR_OFF && !client->borderless)
                                        client->rect.x = event->x - 1;
                                else client->rect.x = event->x - 2;
                        }
                }
                if (event->value_mask & XCB_CONFIG_WINDOW_Y) {
                        if (mode == MODE_STACK || mode == MODE_TABBED) {
                                client->rect.y = event->y - 2;
                        } else {
                                if (client->titlebar_position == TITLEBAR_OFF && client->borderless)
                                        client->rect.y = event->y;
                                else if (client->titlebar_position == TITLEBAR_OFF && !client->borderless)
                                        client->rect.y = event->y - 1;
                                else client->rect.y = event->y - font->height - 2 - 2;
                        }
                }
                if (event->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
                        if (mode == MODE_STACK || mode == MODE_TABBED) {
                                client->rect.width = event->width + 2 + 2;
                        } else {
                                if (client->titlebar_position == TITLEBAR_OFF && client->borderless)
                                        client->rect.width = event->width;
                                else if (client->titlebar_position == TITLEBAR_OFF && !client->borderless)
                                        client->rect.width = event->width + (1 + 1);
                                else client->rect.width = event->width + (2 + 2);
                        }
                }
                if (event->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
                        if (mode == MODE_STACK || mode == MODE_TABBED) {
                                client->rect.height = event->height + 2;
                        } else {
                                if (client->titlebar_position == TITLEBAR_OFF && client->borderless)
                                        client->rect.height = event->height;
                                else if (client->titlebar_position == TITLEBAR_OFF && !client->borderless)
                                        client->rect.height = event->height + (1 + 1);
                                else client->rect.height = event->height + (font->height + 2 + 2) + 2;
                        }
                }

                DLOG("Accepted new position/size for floating client: (%d, %d) size %d x %d\n",
                    client->rect.x, client->rect.y, client->rect.width, client->rect.height);

                /* Push the new position/size to X11 */
                reposition_client(conn, client);
                resize_client(conn, client);
                xcb_flush(conn);

                return 1;
        }

        /* Dock clients can be reconfigured in their height */
        if (client->dock) {
                DLOG("Reconfiguring height of this dock client\n");

                if (!(event->value_mask & XCB_CONFIG_WINDOW_HEIGHT)) {
                        DLOG("Ignoring configure request, no height given\n");
                        return 1;
                }

                client->desired_height = event->height;
                render_workspace(conn, c_ws->output, c_ws);
                xcb_flush(conn);

                return 1;
        }

        if (client->fullscreen) {
                DLOG("Client is in fullscreen mode\n");

                Rect child_rect = client->container->workspace->rect;
                child_rect.x = child_rect.y = 0;
                fake_configure_notify(conn, child_rect, client->child);

                return 1;
        }

        fake_absolute_configure_notify(conn, client);

        return 1;
}

/*
 * Configuration notifies are only handled because we need to set up ignore for
 * the following enter notify events.
 *
 */
int handle_configure_event(void *prophs, xcb_connection_t *conn, xcb_configure_notify_event_t *event) {
        /* We ignore this sequence twice because events for child and frame should be ignored */
        add_ignore_event(event->sequence);
        add_ignore_event(event->sequence);

        return 1;
}

/*
 * Gets triggered upon a RandR screen change event, that is when the user
 * changes the screen configuration in any way (mode, position, …)
 *
 */
int handle_screen_change(void *prophs, xcb_connection_t *conn,
                         xcb_generic_event_t *e) {
        DLOG("RandR screen change\n");

        randr_query_outputs(conn);

        ipc_send_event("output", I3_IPC_EVENT_OUTPUT, "{\"change\":\"unspecified\"}");

        return 1;
}
#endif

/*
 * Our window decorations were unmapped. That means, the window will be killed
 * now, so we better clean up before.
 *
 */
int handle_unmap_notify_event(void *data, xcb_connection_t *conn, xcb_unmap_notify_event_t *event) {

    /* we need to ignore EnterNotify events which will be generated because a
     * different window is visible now */
    add_ignore_event(event->sequence);

    DLOG("UnmapNotify for 0x%08x (received from 0x%08x), serial %d\n", event->window, event->event, event->sequence);
    Con *con = con_by_window_id(event->window);
    if (con == NULL) {
        LOG("Not a managed window, ignoring\n");
        return 1;
    }

    tree_close(con);
    tree_render();
    x_push_changes(croot);
    return 1;

#if 0
        if (client == NULL) {
                DLOG("not a managed window. Ignoring.\n");

                /* This was most likely the destroyed frame of a client which is
                 * currently being unmapped, so we add this sequence (again!) to
                 * the ignore list (enter_notify events will get sent for both,
                 * the child and its frame). */
                add_ignore_event(event->sequence);

                return 0;
        }
#endif


#if 0
        /* Let’s see how many clients there are left on the workspace to delete it if it’s empty */
        bool workspace_empty = SLIST_EMPTY(&(client->workspace->focus_stack));
        bool workspace_focused = (c_ws == client->workspace);
        Client *to_focus = (!workspace_empty ? SLIST_FIRST(&(client->workspace->focus_stack)) : NULL);

        /* If this workspace is currently visible, we don’t delete it */
        if (workspace_is_visible(client->workspace))
                workspace_empty = false;

        if (workspace_empty) {
                client->workspace->output = NULL;
                ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"empty\"}");
        }

        /* Remove the urgency flag if set */
        client->urgent = false;
        workspace_update_urgent_flag(client->workspace);

        render_layout(conn);
#endif

        return 1;
}

#if 0
/*
 * A destroy notify event is sent when the window is not unmapped, but
 * immediately destroyed (for example when starting a window and immediately
 * killing the program which started it).
 *
 * We just pass on the event to the unmap notify handler (by copying the
 * important fields in the event data structure).
 *
 */
int handle_destroy_notify_event(void *data, xcb_connection_t *conn, xcb_destroy_notify_event_t *event) {
        DLOG("destroy notify for 0x%08x, 0x%08x\n", event->event, event->window);

        xcb_unmap_notify_event_t unmap;
        unmap.sequence = event->sequence;
        unmap.event = event->event;
        unmap.window = event->window;

        return handle_unmap_notify_event(NULL, conn, &unmap);
}
#endif
/*
 * Called when a window changes its title
 *
 */
int handle_windowname_change(void *data, xcb_connection_t *conn, uint8_t state,
                                xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *prop) {
    Con *con;
    if ((con = con_by_window_id(window)) == NULL || con->window == NULL)
        return 1;

    window_update_name(con->window, prop);

    x_push_changes(croot);

    return 1;
}

/*
 * Handles legacy window name updates (WM_NAME), see also src/window.c,
 * window_update_name_legacy().
 *
 */
int handle_windowname_change_legacy(void *data, xcb_connection_t *conn, uint8_t state,
                                xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *prop) {
    Con *con;
    if ((con = con_by_window_id(window)) == NULL || con->window == NULL)
        return 1;

    window_update_name_legacy(con->window, prop);

    x_push_changes(croot);

    return 1;
}

/*
 * Updates the client’s WM_CLASS property
 *
 */
int handle_windowclass_change(void *data, xcb_connection_t *conn, uint8_t state,
                             xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *prop) {
    Con *con;
    if ((con = con_by_window_id(window)) == NULL || con->window == NULL)
        return 1;

    window_update_class(con->window, prop);

    return 0;
}

/*
 * Expose event means we should redraw our windows (= title bar)
 *
 */
int handle_expose_event(void *data, xcb_connection_t *conn, xcb_expose_event_t *event) {
    Con *parent, *con;

    /* event->count is the number of minimum remaining expose events for this
     * window, so we skip all events but the last one */
    if (event->count != 0)
        return 1;

    DLOG("window = %08x\n", event->window);

    if ((parent = con_by_frame_id(event->window)) == NULL) {
        LOG("expose event for unknown window, ignoring\n");
        return 1;
    }

    TAILQ_FOREACH(con, &(parent->nodes_head), nodes) {
        LOG("expose for con %p / %s\n", con, con->name);
        if (con->window)
            x_draw_decoration(con);
    }
    xcb_flush(conn);

    return 1;

#if 0
    else {
            uint32_t background_color;
            if (client->urgent)
                    background_color = config.client.urgent.background;
            /* Distinguish if the window is currently focused… */
            else if (CUR_CELL != NULL && CUR_CELL->currently_focused == client)
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
            if (client->titlebar_position == TITLEBAR_OFF && !client->borderless) {
                    xcb_rectangle_t crect = {1, 0, client->rect.width - (1 + 1), client->rect.height - 1};
                    xcb_poly_fill_rectangle(conn, client->frame, client->titlegc, 1, &crect);
            } else {
                    xcb_rectangle_t crect = {2, 0, client->rect.width - (2 + 2), client->rect.height - 2};
                    xcb_poly_fill_rectangle(conn, client->frame, client->titlegc, 1, &crect);
            }
    }
    xcb_flush(conn);
    return 1;
#endif
}

#if 0
/*
 * Handle client messages (EWMH)
 *
 */
int handle_client_message(void *data, xcb_connection_t *conn, xcb_client_message_event_t *event) {
        if (event->type == atoms[_NET_WM_STATE]) {
                if (event->format != 32 || event->data.data32[1] != atoms[_NET_WM_STATE_FULLSCREEN])
                        return 0;

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
                ELOG("unhandled clientmessage\n");
                return 0;
        }

        return 1;
}

int handle_window_type(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                        xcb_atom_t atom, xcb_get_property_reply_t *property) {
        /* TODO: Implement this one. To do this, implement a little test program which sleep(1)s
         before changing this property. */
        ELOG("_NET_WM_WINDOW_TYPE changed, this is not yet implemented.\n");
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
        Client *client = table_get(&by_child, window);
        if (client == NULL) {
                DLOG("Received WM_SIZE_HINTS for unknown client\n");
                return 1;
        }
        xcb_size_hints_t size_hints;

        CLIENT_LOG(client);

        /* If the hints were already in this event, use them, if not, request them */
        if (reply != NULL)
                xcb_get_wm_size_hints_from_reply(&size_hints, reply);
        else
                xcb_get_wm_normal_hints_reply(conn, xcb_get_wm_normal_hints_unchecked(conn, client->child), &size_hints, NULL);

        if ((size_hints.flags & XCB_SIZE_HINT_P_MIN_SIZE)) {
                // TODO: Minimum size is not yet implemented
                DLOG("Minimum size: %d (width) x %d (height)\n", size_hints.min_width, size_hints.min_height);
        }

        bool changed = false;
        if ((size_hints.flags & XCB_SIZE_HINT_P_RESIZE_INC)) {
                if (size_hints.width_inc > 0 && size_hints.width_inc < 0xFFFF)
                        if (client->width_increment != size_hints.width_inc) {
                                client->width_increment = size_hints.width_inc;
                                changed = true;
                        }
                if (size_hints.height_inc > 0 && size_hints.height_inc < 0xFFFF)
                        if (client->height_increment != size_hints.height_inc) {
                                client->height_increment = size_hints.height_inc;
                                changed = true;
                        }

                if (changed)
                        DLOG("resize increments changed\n");
        }

        int base_width = 0, base_height = 0;

        /* base_width/height are the desired size of the window.
           We check if either the program-specified size or the program-specified
           min-size is available */
        if (size_hints.flags & XCB_SIZE_HINT_BASE_SIZE) {
                base_width = size_hints.base_width;
                base_height = size_hints.base_height;
        } else if (size_hints.flags & XCB_SIZE_HINT_P_MIN_SIZE) {
                /* TODO: is this right? icccm says not */
                base_width = size_hints.min_width;
                base_height = size_hints.min_height;
        }

        if (base_width != client->base_width ||
            base_height != client->base_height) {
                client->base_width = base_width;
                client->base_height = base_height;
                DLOG("client's base_height changed to %d\n", base_height);
                DLOG("client's base_width changed to %d\n", base_width);
                changed = true;
        }

        if (changed) {
                if (client->fullscreen)
                        DLOG("Not resizing client, it is in fullscreen mode\n");
                else {
                        resize_client(conn, client);
                        xcb_flush(conn);
                }
        }

        /* If no aspect ratio was set or if it was invalid, we ignore the hints */
        if (!(size_hints.flags & XCB_SIZE_HINT_P_ASPECT) ||
            (size_hints.min_aspect_num <= 0) ||
            (size_hints.min_aspect_den <= 0)) {
                return 1;
        }

        double width = client->rect.width - base_width;
        double height = client->rect.height - base_height;
        /* Convert numerator/denominator to a double */
        double min_aspect = (double)size_hints.min_aspect_num / size_hints.min_aspect_den;
        double max_aspect = (double)size_hints.max_aspect_num / size_hints.min_aspect_den;

        DLOG("Aspect ratio set: minimum %f, maximum %f\n", min_aspect, max_aspect);
        DLOG("width = %f, height = %f\n", width, height);

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
 * Handles the WM_HINTS property for extracting the urgency state of the window.
 *
 */
int handle_hints(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                  xcb_atom_t name, xcb_get_property_reply_t *reply) {
        Client *client = table_get(&by_child, window);
        if (client == NULL) {
                DLOG("Received WM_HINTS for unknown client\n");
                return 1;
        }
        xcb_wm_hints_t hints;

        if (reply != NULL) {
                if (!xcb_get_wm_hints_from_reply(&hints, reply))
                        return 1;
        } else {
                if (!xcb_get_wm_hints_reply(conn, xcb_get_wm_hints_unchecked(conn, client->child), &hints, NULL))
                        return 1;
        }

        Client *last_focused = SLIST_FIRST(&(c_ws->focus_stack));
        if (!client->urgent && client == last_focused) {
                DLOG("Ignoring urgency flag for current client\n");
                return 1;
        }

        /* Update the flag on the client directly */
        client->urgent = (xcb_wm_hints_get_urgency(&hints) != 0);
        CLIENT_LOG(client);
        LOG("Urgency flag changed to %d\n", client->urgent);

        workspace_update_urgent_flag(client->workspace);
        redecorate_window(conn, client);

        /* If the workspace this client is on is not visible, we need to redraw
         * the workspace bar */
        if (!workspace_is_visible(client->workspace)) {
                Output *output = client->workspace->output;
                render_workspace(conn, output, output->current_workspace);
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
        Client *client = table_get(&by_child, window);
        if (client == NULL) {
                DLOG("No such client\n");
                return 1;
        }

        xcb_window_t transient_for;

        if (reply != NULL) {
                if (!xcb_get_wm_transient_for_from_reply(&transient_for, reply))
                        return 1;
        } else {
                if (!xcb_get_wm_transient_for_reply(conn, xcb_get_wm_transient_for_unchecked(conn, window),
                                                    &transient_for, NULL))
                        return 1;
        }

        if (client->floating == FLOATING_AUTO_OFF) {
                DLOG("This is a popup window, putting into floating\n");
                toggle_floating_mode(conn, client, true);
        }

        return 1;
}

/*
 * Handles changes of the WM_CLIENT_LEADER atom which specifies if this is a
 * toolwindow (or similar) and to which window it belongs (logical parent).
 *
 */
int handle_clientleader_change(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                        xcb_atom_t name, xcb_get_property_reply_t *prop) {
        if (prop == NULL) {
                prop = xcb_get_property_reply(conn, xcb_get_property_unchecked(conn,
                                        false, window, WM_CLIENT_LEADER, WINDOW, 0, 32), NULL);
                if (prop == NULL)
                        return 1;
        }

        Client *client = table_get(&by_child, window);
        if (client == NULL)
                return 1;

        xcb_window_t *leader = xcb_get_property_value(prop);
        if (leader == NULL)
                return 1;

        DLOG("Client leader changed to %08x\n", *leader);

        client->leader = *leader;

        return 1;
}
#endif
