/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2010 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include <time.h>

#include <xcb/randr.h>

#include <X11/XKBlib.h>

#include "all.h"

int randr_base = -1;

/* After mapping/unmapping windows, a notify event is generated. However, we don’t want it,
   since it’d trigger an infinite loop of switching between the different windows when
   changing workspaces */
static SLIST_HEAD(ignore_head, Ignore_Event) ignore_events;

/*
 * Adds the given sequence to the list of events which are ignored.
 * If this ignore should only affect a specific response_type, pass
 * response_type, otherwise, pass -1.
 *
 * Every ignored sequence number gets garbage collected after 5 seconds.
 *
 */
void add_ignore_event(const int sequence, const int response_type) {
    struct Ignore_Event *event = smalloc(sizeof(struct Ignore_Event));

    event->sequence = sequence;
    event->response_type = response_type;
    event->added = time(NULL);

    SLIST_INSERT_HEAD(&ignore_events, event, ignore_events);
}

/*
 * Checks if the given sequence is ignored and returns true if so.
 *
 */
bool event_is_ignored(const int sequence, const int response_type) {
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

        if (event->response_type != -1 &&
            event->response_type != response_type)
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
static int handle_key_press(xcb_key_press_event_t *event) {
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

    char *json_result = parse_cmd(bind->command);
    FREE(json_result);
    return 1;
}

/*
 * Called with coordinates of an enter_notify event or motion_notify event
 * to check if the user crossed virtual screen boundaries and adjust the
 * current workspace, if so.
 *
 */
static void check_crossing_screen_boundary(uint32_t x, uint32_t y) {
    Output *output;

    /* If the user disable focus follows mouse, we have nothing to do here */
    if (config.disable_focus_follows_mouse)
        return;

    if ((output = get_output_containing(x, y)) == NULL) {
        ELOG("ERROR: No such screen\n");
        return;
    }

    if (output->con == NULL) {
        ELOG("ERROR: The screen is not recognized by i3 (no container associated)\n");
        return;
    }

    /* Focus the output on which the user moved his cursor */
    Con *old_focused = focused;
    con_focus(con_descend_focused(output_get_content(output->con)));

    /* If the focus changed, we re-render to get updated decorations */
    if (old_focused != focused)
        tree_render();
}

/*
 * When the user moves the mouse pointer onto a window, this callback gets called.
 *
 */
static int handle_enter_notify(xcb_enter_notify_event_t *event) {
    Con *con;

    DLOG("enter_notify for %08x, mode = %d, detail %d, serial %d\n",
         event->event, event->mode, event->detail, event->sequence);
    DLOG("coordinates %d, %d\n", event->event_x, event->event_y);
    if (event->mode != XCB_NOTIFY_MODE_NORMAL) {
        DLOG("This was not a normal notify, ignoring\n");
        return 1;
    }
    /* Some events are not interesting, because they were not generated
     * actively by the user, but by reconfiguration of windows */
    if (event_is_ignored(event->sequence, XCB_ENTER_NOTIFY)) {
        DLOG("Event ignored\n");
        return 1;
    }

    bool enter_child = false;
    /* Get container by frame or by child window */
    if ((con = con_by_frame_id(event->event)) == NULL) {
        con = con_by_window_id(event->event);
        enter_child = true;
    }

    /* If not, then the user moved his cursor to the root window. In that case, we adjust c_ws */
    if (con == NULL) {
        DLOG("Getting screen at %d x %d\n", event->root_x, event->root_y);
        check_crossing_screen_boundary(event->root_x, event->root_y);
        return 1;
    }

    if (con->parent->type == CT_DOCKAREA) {
        DLOG("Ignoring, this is a dock client\n");
        return 1;
    }

    /* see if the user entered the window on a certain window decoration */
    int layout = (enter_child ? con->parent->layout : con->layout);
    if (layout == L_DEFAULT) {
        Con *child;
        TAILQ_FOREACH(child, &(con->nodes_head), nodes)
            if (rect_contains(child->deco_rect, event->event_x, event->event_y)) {
                LOG("using child %p / %s instead!\n", child, child->name);
                con = child;
                break;
            }
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

    con_focus(con_descend_focused(con));
    tree_render();

    return 1;
}

/*
 * When the user moves the mouse but does not change the active window
 * (e.g. when having no windows opened but moving mouse on the root screen
 * and crossing virtual screen boundaries), this callback gets called.
 *
 */
static int handle_motion_notify(xcb_motion_notify_event_t *event) {
    /* Skip events where the pointer was over a child window, we are only
     * interested in events on the root window. */
    if (event->child != 0)
        return 1;

    Con *con;
    if ((con = con_by_frame_id(event->event)) == NULL) {
        check_crossing_screen_boundary(event->root_x, event->root_y);
        return 1;
    }

    if (config.disable_focus_follows_mouse)
        return 1;

    if (con->layout != L_DEFAULT)
        return 1;

    /* see over which rect the user is */
    Con *current;
    TAILQ_FOREACH(current, &(con->nodes_head), nodes) {
        if (!rect_contains(current->deco_rect, event->event_x, event->event_y))
            continue;

        /* We found the rect, let’s see if this window is focused */
        if (TAILQ_FIRST(&(con->focus_head)) == current)
            return 1;

        con_focus(current);
        x_push_changes(croot);
        return 1;
    }

    return 1;
}

/*
 * Called when the keyboard mapping changes (for example by using Xmodmap),
 * we need to update our key bindings then (re-translate symbols).
 *
 */
static int handle_mapping_notify(xcb_mapping_notify_event_t *event) {
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

/*
 * A new window appeared on the screen (=was mapped), so let’s manage it.
 *
 */
static int handle_map_request(xcb_map_request_event_t *event) {
    xcb_get_window_attributes_cookie_t cookie;

    cookie = xcb_get_window_attributes_unchecked(conn, event->window);

    DLOG("window = 0x%08x, serial is %d.\n", event->window, event->sequence);
    add_ignore_event(event->sequence, -1);

    manage_window(event->window, cookie, false);
    x_push_changes(croot);
    return 1;
}

/*
 * Configure requests are received when the application wants to resize windows on their own.
 *
 * We generate a synthethic configure notify event to signalize the client its "new" position.
 *
 */
static int handle_configure_request(xcb_configure_request_event_t *event) {
    Con *con;

    DLOG("window 0x%08x wants to be at %dx%d with %dx%d\n",
        event->window, event->x, event->y, event->width, event->height);

    /* For unmanaged windows, we just execute the configure request. As soon as
     * it gets mapped, we will take over anyways. */
    if ((con = con_by_window_id(event->window)) == NULL) {
        DLOG("Configure request for unmanaged window, can do that.\n");

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

    DLOG("Configure request!\n");
    if (con_is_floating(con) && con_is_leaf(con)) {
        /* find the height for the decorations */
        int deco_height = config.font.height + 5;
        /* we actually need to apply the size/position changes to the *parent*
         * container */
        Rect bsr = con_border_style_rect(con);
        if (con->border_style == BS_NORMAL) {
            bsr.y += deco_height;
            bsr.height -= deco_height;
        }
        Con *floatingcon = con->parent;
        DLOG("Container is a floating leaf node, will do that.\n");
        if (event->value_mask & XCB_CONFIG_WINDOW_X) {
            floatingcon->rect.x = event->x + (-1) * bsr.x;
            DLOG("proposed x = %d, new x is %d\n", event->x, floatingcon->rect.x);
        }
        if (event->value_mask & XCB_CONFIG_WINDOW_Y) {
            floatingcon->rect.y = event->y + (-1) * bsr.y;
            DLOG("proposed y = %d, new y is %d\n", event->y, floatingcon->rect.y);
        }
        if (event->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
            floatingcon->rect.width = event->width + (-1) * bsr.width;
            floatingcon->rect.width += con->border_width * 2;
            DLOG("proposed width = %d, new width is %d (x11 border %d)\n", event->width, floatingcon->rect.width, con->border_width);
        }
        if (event->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
            floatingcon->rect.height = event->height + (-1) * bsr.height;
            floatingcon->rect.height += con->border_width * 2;
            DLOG("proposed height = %d, new height is %d (x11 border %d)\n", event->height, floatingcon->rect.height, con->border_width);
        }
        floating_maybe_reassign_ws(floatingcon);
        tree_render();
    }

    /* Dock windows can be reconfigured in their height */
    if (con->parent && con->parent->type == CT_DOCKAREA) {
        DLOG("Dock window, only height reconfiguration allowed\n");
        if (event->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
            DLOG("Height given, changing\n");

            con->geometry.height = event->height;
            tree_render();
        }
    }

    fake_absolute_configure_notify(con);

    return 1;
}
#if 0

/*
 * Configuration notifies are only handled because we need to set up ignore for
 * the following enter notify events.
 *
 */
int handle_configure_event(void *prophs, xcb_connection_t *conn, xcb_configure_notify_event_t *event) {
    DLOG("configure_event, sequence %d\n", event->sequence);
        /* We ignore this sequence twice because events for child and frame should be ignored */
        add_ignore_event(event->sequence);
        add_ignore_event(event->sequence);

        return 1;
}
#endif

/*
 * Gets triggered upon a RandR screen change event, that is when the user
 * changes the screen configuration in any way (mode, position, …)
 *
 */
static int handle_screen_change(xcb_generic_event_t *e) {
    DLOG("RandR screen change\n");

    randr_query_outputs();

    ipc_send_event("output", I3_IPC_EVENT_OUTPUT, "{\"change\":\"unspecified\"}");

    return 1;
}

/*
 * Our window decorations were unmapped. That means, the window will be killed
 * now, so we better clean up before.
 *
 */
static int handle_unmap_notify_event(xcb_unmap_notify_event_t *event) {
    // XXX: this is commented out because in src/x.c we disable EnterNotify events
    /* we need to ignore EnterNotify events which will be generated because a
     * different window is visible now */
    //add_ignore_event(event->sequence, XCB_ENTER_NOTIFY);

    DLOG("UnmapNotify for 0x%08x (received from 0x%08x), serial %d\n", event->window, event->event, event->sequence);
    Con *con = con_by_window_id(event->window);
    if (con == NULL) {
        /* This could also be an UnmapNotify for the frame. We need to
         * decrement the ignore_unmap counter. */
        con = con_by_frame_id(event->window);
        if (con == NULL) {
            LOG("Not a managed window, ignoring UnmapNotify event\n");
            return 1;
        }
        if (con->ignore_unmap > 0)
            con->ignore_unmap--;
        DLOG("ignore_unmap = %d for frame of container %p\n", con->ignore_unmap, con);
        return 1;
    }

    if (con->ignore_unmap > 0) {
        DLOG("ignore_unmap = %d, dec\n", con->ignore_unmap);
        con->ignore_unmap--;
        return 1;
    }

    tree_close(con, DONT_KILL_WINDOW, false, false);
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

/*
 * A destroy notify event is sent when the window is not unmapped, but
 * immediately destroyed (for example when starting a window and immediately
 * killing the program which started it).
 *
 * We just pass on the event to the unmap notify handler (by copying the
 * important fields in the event data structure).
 *
 */
static int handle_destroy_notify_event(xcb_destroy_notify_event_t *event) {
    DLOG("destroy notify for 0x%08x, 0x%08x\n", event->event, event->window);

    xcb_unmap_notify_event_t unmap;
    unmap.sequence = event->sequence;
    unmap.event = event->event;
    unmap.window = event->window;

    return handle_unmap_notify_event(&unmap);
}

/*
 * Called when a window changes its title
 *
 */
static bool handle_windowname_change(void *data, xcb_connection_t *conn, uint8_t state,
                                xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *prop) {
    Con *con;
    if ((con = con_by_window_id(window)) == NULL || con->window == NULL)
        return false;

    window_update_name(con->window, prop, false);

    x_push_changes(croot);

    return true;
}

/*
 * Handles legacy window name updates (WM_NAME), see also src/window.c,
 * window_update_name_legacy().
 *
 */
static bool handle_windowname_change_legacy(void *data, xcb_connection_t *conn, uint8_t state,
                                xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *prop) {
    Con *con;
    if ((con = con_by_window_id(window)) == NULL || con->window == NULL)
        return false;

    window_update_name_legacy(con->window, prop, false);

    x_push_changes(croot);

    return true;
}

/*
 * Called when a window changes its WM_WINDOW_ROLE.
 *
 */
static bool handle_windowrole_change(void *data, xcb_connection_t *conn, uint8_t state,
                                     xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *prop) {
    Con *con;
    if ((con = con_by_window_id(window)) == NULL || con->window == NULL)
        return false;

    window_update_role(con->window, prop, false);

    return true;
}

#if 0
/*
 * Updates the client’s WM_CLASS property
 *
 */
static int handle_windowclass_change(void *data, xcb_connection_t *conn, uint8_t state,
                             xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *prop) {
    Con *con;
    if ((con = con_by_window_id(window)) == NULL || con->window == NULL)
        return 1;

    window_update_class(con->window, prop, false);

    return 0;
}
#endif

/*
 * Expose event means we should redraw our windows (= title bar)
 *
 */
static int handle_expose_event(xcb_expose_event_t *event) {
    Con *parent;

    /* event->count is the number of minimum remaining expose events for this
     * window, so we skip all events but the last one */
    if (event->count != 0)
        return 1;

    DLOG("window = %08x\n", event->window);

    if ((parent = con_by_frame_id(event->window)) == NULL) {
        LOG("expose event for unknown window, ignoring\n");
        return 1;
    }

    /* re-render the parent (recursively, if it’s a split con) */
    x_deco_recurse(parent);
    xcb_flush(conn);

    return 1;
}

/*
 * Handle client messages (EWMH)
 *
 */
static int handle_client_message(xcb_client_message_event_t *event) {
    LOG("ClientMessage for window 0x%08x\n", event->window);
    if (event->type == A__NET_WM_STATE) {
        if (event->format != 32 || event->data.data32[1] != A__NET_WM_STATE_FULLSCREEN) {
            DLOG("atom in clientmessage is %d, fullscreen is %d\n",
                    event->data.data32[1], A__NET_WM_STATE_FULLSCREEN);
            DLOG("not about fullscreen atom\n");
            return 0;
        }

        Con *con = con_by_window_id(event->window);
        if (con == NULL) {
            DLOG("Could not get window for client message\n");
            return 0;
        }

        /* Check if the fullscreen state should be toggled */
        if ((con->fullscreen_mode != CF_NONE &&
             (event->data.data32[0] == _NET_WM_STATE_REMOVE ||
              event->data.data32[0] == _NET_WM_STATE_TOGGLE)) ||
            (con->fullscreen_mode == CF_NONE &&
             (event->data.data32[0] == _NET_WM_STATE_ADD ||
              event->data.data32[0] == _NET_WM_STATE_TOGGLE))) {
            DLOG("toggling fullscreen\n");
            con_toggle_fullscreen(con, CF_OUTPUT);
        }

        tree_render();
        x_push_changes(croot);
    } else if (event->type == A_I3_SYNC) {
        DLOG("i3 sync, yay\n");
        xcb_window_t window = event->data.data32[0];
        uint32_t rnd = event->data.data32[1];
        DLOG("Sending random value %d back to X11 window 0x%08x\n", rnd, window);

        void *reply = scalloc(32);
        xcb_client_message_event_t *ev = reply;

        ev->response_type = XCB_CLIENT_MESSAGE;
        ev->window = window;
        ev->type = A_I3_SYNC;
        ev->format = 32;
        ev->data.data32[0] = window;
        ev->data.data32[1] = rnd;

        xcb_send_event(conn, false, window, XCB_EVENT_MASK_NO_EVENT, (char*)ev);
        xcb_flush(conn);
        free(reply);
    } else {
        ELOG("unhandled clientmessage\n");
        return 0;
    }

    return 1;
}

#if 0
int handle_window_type(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                        xcb_atom_t atom, xcb_get_property_reply_t *property) {
        /* TODO: Implement this one. To do this, implement a little test program which sleep(1)s
         before changing this property. */
        ELOG("_NET_WM_WINDOW_TYPE changed, this is not yet implemented.\n");
        return 0;
}
#endif

/*
 * Handles the size hints set by a window, but currently only the part necessary for displaying
 * clients proportionally inside their frames (mplayer for example)
 *
 * See ICCCM 4.1.2.3 for more details
 *
 */
static bool handle_normal_hints(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                        xcb_atom_t name, xcb_get_property_reply_t *reply) {
    Con *con = con_by_window_id(window);
    if (con == NULL) {
        DLOG("Received WM_NORMAL_HINTS for unknown client\n");
        return false;
    }

    xcb_size_hints_t size_hints;

        //CLIENT_LOG(client);

    /* If the hints were already in this event, use them, if not, request them */
    if (reply != NULL)
        xcb_icccm_get_wm_size_hints_from_reply(&size_hints, reply);
    else
        xcb_icccm_get_wm_normal_hints_reply(conn, xcb_icccm_get_wm_normal_hints_unchecked(conn, con->window->id), &size_hints, NULL);

    if ((size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
        // TODO: Minimum size is not yet implemented
        DLOG("Minimum size: %d (width) x %d (height)\n", size_hints.min_width, size_hints.min_height);
    }

    bool changed = false;
    if ((size_hints.flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC)) {
        if (size_hints.width_inc > 0 && size_hints.width_inc < 0xFFFF)
            if (con->width_increment != size_hints.width_inc) {
                con->width_increment = size_hints.width_inc;
                changed = true;
            }
        if (size_hints.height_inc > 0 && size_hints.height_inc < 0xFFFF)
            if (con->height_increment != size_hints.height_inc) {
                con->height_increment = size_hints.height_inc;
                changed = true;
            }

        if (changed)
            DLOG("resize increments changed\n");
    }

    int base_width = 0, base_height = 0;

    /* base_width/height are the desired size of the window.
       We check if either the program-specified size or the program-specified
       min-size is available */
    if (size_hints.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) {
        base_width = size_hints.base_width;
        base_height = size_hints.base_height;
    } else if (size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) {
        /* TODO: is this right? icccm says not */
        base_width = size_hints.min_width;
        base_height = size_hints.min_height;
    }

    if (base_width != con->base_width ||
        base_height != con->base_height) {
        con->base_width = base_width;
        con->base_height = base_height;
        DLOG("client's base_height changed to %d\n", base_height);
        DLOG("client's base_width changed to %d\n", base_width);
        changed = true;
    }

    /* If no aspect ratio was set or if it was invalid, we ignore the hints */
    if (!(size_hints.flags & XCB_ICCCM_SIZE_HINT_P_ASPECT) ||
        (size_hints.min_aspect_num <= 0) ||
        (size_hints.min_aspect_den <= 0)) {
        goto render_and_return;
    }

    /* XXX: do we really use rect here, not window_rect? */
    double width = con->rect.width - base_width;
    double height = con->rect.height - base_height;
    /* Convert numerator/denominator to a double */
    double min_aspect = (double)size_hints.min_aspect_num / size_hints.min_aspect_den;
    double max_aspect = (double)size_hints.max_aspect_num / size_hints.min_aspect_den;

    DLOG("Aspect ratio set: minimum %f, maximum %f\n", min_aspect, max_aspect);
    DLOG("width = %f, height = %f\n", width, height);

    /* Sanity checks, this is user-input, in a way */
    if (max_aspect <= 0 || min_aspect <= 0 || height == 0 || (width / height) <= 0)
        goto render_and_return;

    /* Check if we need to set proportional_* variables using the correct ratio */
    if ((width / height) < min_aspect) {
        if (con->proportional_width != width ||
            con->proportional_height != (width / min_aspect)) {
            con->proportional_width = width;
            con->proportional_height = width / min_aspect;
            changed = true;
        }
    } else if ((width / height) > max_aspect) {
        if (con->proportional_width != width ||
            con->proportional_height != (width / max_aspect)) {
            con->proportional_width = width;
            con->proportional_height = width / max_aspect;
            changed = true;
        }
    } else goto render_and_return;

render_and_return:
    if (changed)
        tree_render();
    FREE(reply);
    return true;
}

/*
 * Handles the WM_HINTS property for extracting the urgency state of the window.
 *
 */
static bool handle_hints(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                  xcb_atom_t name, xcb_get_property_reply_t *reply) {
    Con *con = con_by_window_id(window);
    if (con == NULL) {
        DLOG("Received WM_HINTS for unknown client\n");
        return false;
    }

    xcb_icccm_wm_hints_t hints;

    if (reply != NULL) {
        if (!xcb_icccm_get_wm_hints_from_reply(&hints, reply))
            return false;
    } else {
        if (!xcb_icccm_get_wm_hints_reply(conn, xcb_icccm_get_wm_hints_unchecked(conn, con->window->id), &hints, NULL))
            return false;
    }

    if (!con->urgent && focused == con) {
        DLOG("Ignoring urgency flag for current client\n");
        FREE(reply);
        return true;
    }

    /* Update the flag on the client directly */
    con->urgent = (xcb_icccm_wm_hints_get_urgency(&hints) != 0);
    //CLIENT_LOG(con);
    LOG("Urgency flag changed to %d\n", con->urgent);

    Con *ws;
    /* Set the urgency flag on the workspace, if a workspace could be found
     * (for dock clients, that is not the case). */
    if ((ws = con_get_workspace(con)) != NULL)
        workspace_update_urgent_flag(ws);

    tree_render();

#if 0
    /* If the workspace this client is on is not visible, we need to redraw
     * the workspace bar */
    if (!workspace_is_visible(client->workspace)) {
            Output *output = client->workspace->output;
            render_workspace(conn, output, output->current_workspace);
            xcb_flush(conn);
    }
#endif

    FREE(reply);
    return true;
}

/*
 * Handles the transient for hints set by a window, signalizing that this window is a popup window
 * for some other window.
 *
 * See ICCCM 4.1.2.6 for more details
 *
 */
static bool handle_transient_for(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                         xcb_atom_t name, xcb_get_property_reply_t *prop) {
    Con *con;

    if ((con = con_by_window_id(window)) == NULL || con->window == NULL) {
        DLOG("No such window\n");
        return false;
    }

    if (prop == NULL) {
        prop = xcb_get_property_reply(conn, xcb_get_property_unchecked(conn,
                                false, window, XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, 0, 32), NULL);
        if (prop == NULL)
            return false;
    }

    window_update_transient_for(con->window, prop);

    // TODO: put window in floating mode if con->window->transient_for != XCB_NONE:
#if 0
    if (client->floating == FLOATING_AUTO_OFF) {
        DLOG("This is a popup window, putting into floating\n");
        toggle_floating_mode(conn, client, true);
    }
#endif

    return true;
}

/*
 * Handles changes of the WM_CLIENT_LEADER atom which specifies if this is a
 * toolwindow (or similar) and to which window it belongs (logical parent).
 *
 */
static bool handle_clientleader_change(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                        xcb_atom_t name, xcb_get_property_reply_t *prop) {
    Con *con;
    if ((con = con_by_window_id(window)) == NULL || con->window == NULL)
        return false;

    if (prop == NULL) {
        prop = xcb_get_property_reply(conn, xcb_get_property_unchecked(conn,
                                false, window, A_WM_CLIENT_LEADER, XCB_ATOM_WINDOW, 0, 32), NULL);
        if (prop == NULL)
            return false;
    }

    window_update_leader(con->window, prop);

    return true;
}

/*
 * Handles FocusIn events which are generated by clients (i3’s focus changes
 * don’t generate FocusIn events due to a different EventMask) and updates the
 * decorations accordingly.
 *
 */
static int handle_focus_in(xcb_focus_in_event_t *event) {
    DLOG("focus change in, for window 0x%08x\n", event->event);
    Con *con;
    if ((con = con_by_window_id(event->event)) == NULL || con->window == NULL)
        return 1;
    DLOG("That is con %p / %s\n", con, con->name);

    if (event->mode == XCB_NOTIFY_MODE_GRAB ||
        event->mode == XCB_NOTIFY_MODE_UNGRAB) {
        DLOG("FocusIn event for grab/ungrab, ignoring\n");
        return 1;
    }

    if (event->detail == XCB_NOTIFY_DETAIL_POINTER) {
        DLOG("notify detail is pointer, ignoring this event\n");
        return 1;
    }

    if (focused_id == event->event) {
        DLOG("focus matches the currently focused window, not doing anything\n");
        return 1;
    }

    DLOG("focus is different, updating decorations\n");
    con_focus(con);
    /* We update focused_id because we don’t need to set focus again */
    focused_id = event->event;
    x_push_changes(croot);
    return 1;
}

/* Returns false if the event could not be processed (e.g. the window could not
 * be found), true otherwise */
typedef bool (*cb_property_handler_t)(void *data, xcb_connection_t *c, uint8_t state, xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *property);

struct property_handler_t {
    xcb_atom_t atom;
    uint32_t long_len;
    cb_property_handler_t cb;
};

static struct property_handler_t property_handlers[] = {
    { 0, 128, handle_windowname_change },
    { 0, UINT_MAX, handle_hints },
    { 0, 128, handle_windowname_change_legacy },
    { 0, UINT_MAX, handle_normal_hints },
    { 0, UINT_MAX, handle_clientleader_change },
    { 0, UINT_MAX, handle_transient_for },
    { 0, 128, handle_windowrole_change }
};
#define NUM_HANDLERS (sizeof(property_handlers) / sizeof(struct property_handler_t))

/*
 * Sets the appropriate atoms for the property handlers after the atoms were
 * received from X11
 *
 */
void property_handlers_init() {
    property_handlers[0].atom = A__NET_WM_NAME;
    property_handlers[1].atom = XCB_ATOM_WM_HINTS;
    property_handlers[2].atom = XCB_ATOM_WM_NAME;
    property_handlers[3].atom = XCB_ATOM_WM_NORMAL_HINTS;
    property_handlers[4].atom = A_WM_CLIENT_LEADER;
    property_handlers[5].atom = XCB_ATOM_WM_TRANSIENT_FOR;
    property_handlers[6].atom = A_WM_WINDOW_ROLE;
}

static void property_notify(uint8_t state, xcb_window_t window, xcb_atom_t atom) {
    struct property_handler_t *handler = NULL;
    xcb_get_property_reply_t *propr = NULL;

    for (int c = 0; c < sizeof(property_handlers) / sizeof(struct property_handler_t); c++) {
        if (property_handlers[c].atom != atom)
            continue;

        handler = &property_handlers[c];
        break;
    }

    if (handler == NULL) {
        DLOG("Unhandled property notify for atom %d (0x%08x)\n", atom, atom);
        return;
    }

    if (state != XCB_PROPERTY_DELETE) {
        xcb_get_property_cookie_t cookie = xcb_get_property(conn, 0, window, atom, XCB_GET_PROPERTY_TYPE_ANY, 0, handler->long_len);
        propr = xcb_get_property_reply(conn, cookie, 0);
    }

    /* the handler will free() the reply unless it returns false */
    if (!handler->cb(NULL, conn, state, window, atom, propr))
        FREE(propr);
}

/*
 * Takes an xcb_generic_event_t and calls the appropriate handler, based on the
 * event type.
 *
 */
void handle_event(int type, xcb_generic_event_t *event) {
    if (randr_base > -1 &&
        type == randr_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
        handle_screen_change(event);
        return;
    }

    switch (type) {
        case XCB_KEY_PRESS:
            handle_key_press((xcb_key_press_event_t*)event);
            break;

        case XCB_BUTTON_PRESS:
            handle_button_press((xcb_button_press_event_t*)event);
            break;

        case XCB_MAP_REQUEST:
            handle_map_request((xcb_map_request_event_t*)event);
            break;

        case XCB_UNMAP_NOTIFY:
            handle_unmap_notify_event((xcb_unmap_notify_event_t*)event);
            break;

        case XCB_DESTROY_NOTIFY:
            handle_destroy_notify_event((xcb_destroy_notify_event_t*)event);
            break;

        case XCB_EXPOSE:
            handle_expose_event((xcb_expose_event_t*)event);
            break;

        case XCB_MOTION_NOTIFY:
            handle_motion_notify((xcb_motion_notify_event_t*)event);
            break;

        /* Enter window = user moved his mouse over the window */
        case XCB_ENTER_NOTIFY:
            handle_enter_notify((xcb_enter_notify_event_t*)event);
            break;

        /* Client message are sent to the root window. The only interesting
         * client message for us is _NET_WM_STATE, we honour
         * _NET_WM_STATE_FULLSCREEN */
        case XCB_CLIENT_MESSAGE:
            handle_client_message((xcb_client_message_event_t*)event);
            break;

        /* Configure request = window tried to change size on its own */
        case XCB_CONFIGURE_REQUEST:
            handle_configure_request((xcb_configure_request_event_t*)event);
            break;

        /* Mapping notify = keyboard mapping changed (Xmodmap), re-grab bindings */
        case XCB_MAPPING_NOTIFY:
            handle_mapping_notify((xcb_mapping_notify_event_t*)event);
            break;

        case XCB_FOCUS_IN:
            handle_focus_in((xcb_focus_in_event_t*)event);
            break;

        case XCB_PROPERTY_NOTIFY:
            DLOG("Property notify\n");
            xcb_property_notify_event_t *e = (xcb_property_notify_event_t*)event;
            property_notify(e->state, e->window, e->atom);
            break;

        default:
            DLOG("Unhandled event of type %d\n", type);
            break;
    }
}
