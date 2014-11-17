#undef I3__FILE__
#define I3__FILE__ "handlers.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * handlers.c: Small handlers for various events (keypresses, focus changes,
 *             …).
 *
 */
#include "all.h"

#include <time.h>
#include <float.h>
#include <sys/time.h>
#include <xcb/randr.h>
#include <X11/XKBlib.h>
#define SN_API_NOT_YET_FROZEN 1
#include <libsn/sn-monitor.h>

int randr_base = -1;
int xkb_base = -1;
int xkb_current_group;

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
        } else
            event = SLIST_NEXT(event, ignore_events);
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
    Con *next = con_descend_focused(output_get_content(output->con));
    /* Since we are switching outputs, this *must* be a different workspace, so
     * call workspace_show() */
    workspace_show(con_get_workspace(next));
    con_focus(next);

    /* If the focus changed, we re-render to get updated decorations */
    if (old_focused != focused)
        tree_render();
}

/*
 * When the user moves the mouse pointer onto a window, this callback gets called.
 *
 */
static void handle_enter_notify(xcb_enter_notify_event_t *event) {
    Con *con;

    last_timestamp = event->time;

    DLOG("enter_notify for %08x, mode = %d, detail %d, serial %d\n",
         event->event, event->mode, event->detail, event->sequence);
    DLOG("coordinates %d, %d\n", event->event_x, event->event_y);
    if (event->mode != XCB_NOTIFY_MODE_NORMAL) {
        DLOG("This was not a normal notify, ignoring\n");
        return;
    }
    /* Some events are not interesting, because they were not generated
     * actively by the user, but by reconfiguration of windows */
    if (event_is_ignored(event->sequence, XCB_ENTER_NOTIFY)) {
        DLOG("Event ignored\n");
        return;
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
        return;
    }

    if (con->parent->type == CT_DOCKAREA) {
        DLOG("Ignoring, this is a dock client\n");
        return;
    }

    /* see if the user entered the window on a certain window decoration */
    layout_t layout = (enter_child ? con->parent->layout : con->layout);
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
        return;

    /* if this container is already focused, there is nothing to do. */
    if (con == focused)
        return;

    /* Get the currently focused workspace to check if the focus change also
     * involves changing workspaces. If so, we need to call workspace_show() to
     * correctly update state and send the IPC event. */
    Con *ws = con_get_workspace(con);
    if (ws != con_get_workspace(focused))
        workspace_show(ws);

    focused_id = XCB_NONE;
    con_focus(con_descend_focused(con));
    tree_render();

    return;
}

/*
 * When the user moves the mouse but does not change the active window
 * (e.g. when having no windows opened but moving mouse on the root screen
 * and crossing virtual screen boundaries), this callback gets called.
 *
 */
static void handle_motion_notify(xcb_motion_notify_event_t *event) {
    last_timestamp = event->time;

    /* Skip events where the pointer was over a child window, we are only
     * interested in events on the root window. */
    if (event->child != 0)
        return;

    Con *con;
    if ((con = con_by_frame_id(event->event)) == NULL) {
        DLOG("MotionNotify for an unknown container, checking if it crosses screen boundaries.\n");
        check_crossing_screen_boundary(event->root_x, event->root_y);
        return;
    }

    if (config.disable_focus_follows_mouse)
        return;

    if (con->layout != L_DEFAULT)
        return;

    /* see over which rect the user is */
    Con *current;
    TAILQ_FOREACH(current, &(con->nodes_head), nodes) {
        if (!rect_contains(current->deco_rect, event->event_x, event->event_y))
            continue;

        /* We found the rect, let’s see if this window is focused */
        if (TAILQ_FIRST(&(con->focus_head)) == current)
            return;

        con_focus(current);
        x_push_changes(croot);
        return;
    }

    return;
}

/*
 * Called when the keyboard mapping changes (for example by using Xmodmap),
 * we need to update our key bindings then (re-translate symbols).
 *
 */
static void handle_mapping_notify(xcb_mapping_notify_event_t *event) {
    if (event->request != XCB_MAPPING_KEYBOARD &&
        event->request != XCB_MAPPING_MODIFIER)
        return;

    DLOG("Received mapping_notify for keyboard or modifier mapping, re-grabbing keys\n");
    xcb_refresh_keyboard_mapping(keysyms, event);

    xcb_numlock_mask = aio_get_mod_mask_for(XCB_NUM_LOCK, keysyms);

    ungrab_all_keys(conn);
    translate_keysyms();
    grab_all_keys(conn, false);

    return;
}

/*
 * A new window appeared on the screen (=was mapped), so let’s manage it.
 *
 */
static void handle_map_request(xcb_map_request_event_t *event) {
    xcb_get_window_attributes_cookie_t cookie;

    cookie = xcb_get_window_attributes_unchecked(conn, event->window);

    DLOG("window = 0x%08x, serial is %d.\n", event->window, event->sequence);
    add_ignore_event(event->sequence, -1);

    manage_window(event->window, cookie, false);
    return;
}

/*
 * Configure requests are received when the application wants to resize windows
 * on their own.
 *
 * We generate a synthethic configure notify event to signalize the client its
 * "new" position.
 *
 */
static void handle_configure_request(xcb_configure_request_event_t *event) {
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
#define COPY_MASK_MEMBER(mask_member, event_member) \
    do {                                            \
        if (event->value_mask & mask_member) {      \
            mask |= mask_member;                    \
            values[c++] = event->event_member;      \
        }                                           \
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

        return;
    }

    DLOG("Configure request!\n");

    Con *workspace = con_get_workspace(con),
        *fullscreen = NULL;

    /* There might not be a corresponding workspace for dock cons, therefore we
     * have to be careful here. */
    if (workspace) {
        fullscreen = con_get_fullscreen_con(workspace, CF_OUTPUT);
        if (!fullscreen)
            fullscreen = con_get_fullscreen_con(workspace, CF_GLOBAL);
    }

    if (fullscreen != con && con_is_floating(con) && con_is_leaf(con)) {
        /* find the height for the decorations */
        int deco_height = con->deco_rect.height;
        /* we actually need to apply the size/position changes to the *parent*
         * container */
        Rect bsr = con_border_style_rect(con);
        if (con->border_style == BS_NORMAL) {
            bsr.y += deco_height;
            bsr.height -= deco_height;
        }
        Con *floatingcon = con->parent;

        if (strcmp(con_get_workspace(floatingcon)->name, "__i3_scratch") == 0) {
            DLOG("This is a scratchpad container, ignoring ConfigureRequest\n");
            return;
        }

        Rect newrect = floatingcon->rect;

        if (event->value_mask & XCB_CONFIG_WINDOW_X) {
            newrect.x = event->x + (-1) * bsr.x;
            DLOG("proposed x = %d, new x is %d\n", event->x, newrect.x);
        }
        if (event->value_mask & XCB_CONFIG_WINDOW_Y) {
            newrect.y = event->y + (-1) * bsr.y;
            DLOG("proposed y = %d, new y is %d\n", event->y, newrect.y);
        }
        if (event->value_mask & XCB_CONFIG_WINDOW_WIDTH) {
            newrect.width = event->width + (-1) * bsr.width;
            newrect.width += con->border_width * 2;
            DLOG("proposed width = %d, new width is %d (x11 border %d)\n",
                 event->width, newrect.width, con->border_width);
        }
        if (event->value_mask & XCB_CONFIG_WINDOW_HEIGHT) {
            newrect.height = event->height + (-1) * bsr.height;
            newrect.height += con->border_width * 2;
            DLOG("proposed height = %d, new height is %d (x11 border %d)\n",
                 event->height, newrect.height, con->border_width);
        }

        DLOG("Container is a floating leaf node, will do that.\n");
        floating_reposition(floatingcon, newrect);
        return;
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

    return;
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
static void handle_screen_change(xcb_generic_event_t *e) {
    DLOG("RandR screen change\n");

    /* The geometry of the root window is used for “fullscreen global” and
     * changes when new outputs are added. */
    xcb_get_geometry_cookie_t cookie = xcb_get_geometry(conn, root);
    xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(conn, cookie, NULL);
    if (reply == NULL) {
        ELOG("Could not get geometry of the root window, exiting\n");
        exit(1);
    }
    DLOG("root geometry reply: (%d, %d) %d x %d\n", reply->x, reply->y, reply->width, reply->height);

    croot->rect.width = reply->width;
    croot->rect.height = reply->height;

    randr_query_outputs();

    scratchpad_fix_resolution();

    ipc_send_event("output", I3_IPC_EVENT_OUTPUT, "{\"change\":\"unspecified\"}");

    return;
}

/*
 * Our window decorations were unmapped. That means, the window will be killed
 * now, so we better clean up before.
 *
 */
static void handle_unmap_notify_event(xcb_unmap_notify_event_t *event) {
    DLOG("UnmapNotify for 0x%08x (received from 0x%08x), serial %d\n", event->window, event->event, event->sequence);
    xcb_get_input_focus_cookie_t cookie;
    Con *con = con_by_window_id(event->window);
    if (con == NULL) {
        /* This could also be an UnmapNotify for the frame. We need to
         * decrement the ignore_unmap counter. */
        con = con_by_frame_id(event->window);
        if (con == NULL) {
            LOG("Not a managed window, ignoring UnmapNotify event\n");
            return;
        }

        if (con->ignore_unmap > 0)
            con->ignore_unmap--;
        /* See the end of this function. */
        cookie = xcb_get_input_focus(conn);
        DLOG("ignore_unmap = %d for frame of container %p\n", con->ignore_unmap, con);
        goto ignore_end;
    }

    /* See the end of this function. */
    cookie = xcb_get_input_focus(conn);

    if (con->ignore_unmap > 0) {
        DLOG("ignore_unmap = %d, dec\n", con->ignore_unmap);
        con->ignore_unmap--;
        goto ignore_end;
    }

    tree_close(con, DONT_KILL_WINDOW, false, false);
    tree_render();

ignore_end:
    /* If the client (as opposed to i3) destroyed or unmapped a window, an
     * EnterNotify event will follow (indistinguishable from an EnterNotify
     * event caused by moving your mouse), causing i3 to set focus to whichever
     * window is now visible.
     *
     * In a complex stacked or tabbed layout (take two v-split containers in a
     * tabbed container), when the bottom window in tab2 is closed, the bottom
     * window of tab1 is visible instead. X11 will thus send an EnterNotify
     * event for the bottom window of tab1, while the focus should be set to
     * the remaining window of tab2.
     *
     * Therefore, we ignore all EnterNotify events which have the same sequence
     * as an UnmapNotify event. */
    add_ignore_event(event->sequence, XCB_ENTER_NOTIFY);

    /* Since we just ignored the sequence of this UnmapNotify, we want to make
     * sure that following events use a different sequence. When putting xterm
     * into fullscreen and moving the pointer to a different window, without
     * using GetInputFocus, subsequent (legitimate) EnterNotify events arrived
     * with the same sequence and thus were ignored (see ticket #609). */
    free(xcb_get_input_focus_reply(conn, cookie, NULL));
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
static void handle_destroy_notify_event(xcb_destroy_notify_event_t *event) {
    DLOG("destroy notify for 0x%08x, 0x%08x\n", event->event, event->window);

    xcb_unmap_notify_event_t unmap;
    unmap.sequence = event->sequence;
    unmap.event = event->event;
    unmap.window = event->window;

    handle_unmap_notify_event(&unmap);
}

static bool window_name_changed(i3Window *window, char *old_name) {
    if ((old_name == NULL) && (window->name == NULL))
        return false;

    /* Either the old or the new one is NULL, but not both. */
    if ((old_name == NULL) ^ (window->name == NULL))
        return true;

    return (strcmp(old_name, i3string_as_utf8(window->name)) != 0);
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

    char *old_name = (con->window->name != NULL ? sstrdup(i3string_as_utf8(con->window->name)) : NULL);

    window_update_name(con->window, prop, false);

    x_push_changes(croot);

    if (window_name_changed(con->window, old_name))
        ipc_send_window_event("title", con);

    FREE(old_name);

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

    char *old_name = (con->window->name != NULL ? sstrdup(i3string_as_utf8(con->window->name)) : NULL);

    window_update_name_legacy(con->window, prop, false);

    x_push_changes(croot);

    if (window_name_changed(con->window, old_name))
        ipc_send_window_event("title", con);

    FREE(old_name);

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
static void handle_expose_event(xcb_expose_event_t *event) {
    Con *parent;

    DLOG("window = %08x\n", event->window);

    if ((parent = con_by_frame_id(event->window)) == NULL) {
        LOG("expose event for unknown window, ignoring\n");
        return;
    }

    /* Since we render to our pixmap on every change anyways, expose events
     * only tell us that the X server lost (parts of) the window contents. We
     * can handle that by copying the appropriate part from our pixmap to the
     * window. */
    xcb_copy_area(conn, parent->pixmap, parent->frame, parent->pm_gc,
                  event->x, event->y, event->x, event->y,
                  event->width, event->height);
    xcb_flush(conn);

    return;
}

/*
 * Handle client messages (EWMH)
 *
 */
static void handle_client_message(xcb_client_message_event_t *event) {
    /* If this is a startup notification ClientMessage, the library will handle
     * it and call our monitor_event() callback. */
    if (sn_xcb_display_process_event(sndisplay, (xcb_generic_event_t *)event))
        return;

    LOG("ClientMessage for window 0x%08x\n", event->window);
    if (event->type == A__NET_WM_STATE) {
        if (event->format != 32 ||
            (event->data.data32[1] != A__NET_WM_STATE_FULLSCREEN &&
             event->data.data32[1] != A__NET_WM_STATE_DEMANDS_ATTENTION)) {
            DLOG("Unknown atom in clientmessage of type %d\n", event->data.data32[1]);
            return;
        }

        Con *con = con_by_window_id(event->window);
        if (con == NULL) {
            DLOG("Could not get window for client message\n");
            return;
        }

        if (event->data.data32[1] == A__NET_WM_STATE_FULLSCREEN) {
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
        } else if (event->data.data32[1] == A__NET_WM_STATE_DEMANDS_ATTENTION) {
            /* Check if the urgent flag must be set or not */
            if (event->data.data32[0] == _NET_WM_STATE_ADD)
                con_set_urgency(con, true);
            else if (event->data.data32[0] == _NET_WM_STATE_REMOVE)
                con_set_urgency(con, false);
            else if (event->data.data32[0] == _NET_WM_STATE_TOGGLE)
                con_set_urgency(con, !con->urgent);
        }

        tree_render();
    } else if (event->type == A__NET_ACTIVE_WINDOW) {
        if (event->format != 32)
            return;

        DLOG("_NET_ACTIVE_WINDOW: Window 0x%08x should be activated\n", event->window);

        Con *con = con_by_window_id(event->window);
        if (con == NULL) {
            DLOG("Could not get window for client message\n");
            return;
        }

        Con *ws = con_get_workspace(con);

        if (ws == NULL) {
            DLOG("Window is not being managed, ignoring _NET_ACTIVE_WINDOW\n");
            return;
        }

        if (con_is_internal(ws)) {
            DLOG("Workspace is internal, ignoring _NET_ACTIVE_WINDOW\n");
            return;
        }

        /* data32[0] indicates the source of the request (application or pager) */
        if (event->data.data32[0] == 2) {
            /* Always focus the con if it is from a pager, because this is most
             * likely from some user action */
            DLOG("This request came from a pager. Focusing con = %p\n", con);
            workspace_show(ws);
            con_focus(con);
        } else {
            /* If the request is from an application, only focus if the
             * workspace is visible. Otherwise set the urgency hint. */
            if (workspace_is_visible(ws)) {
                DLOG("Request to focus con on a visible workspace. Focusing con = %p\n", con);
                workspace_show(ws);
                con_focus(con);
            } else {
                DLOG("Request to focus con on a hidden workspace. Setting urgent con = %p\n", con);
                con_set_urgency(con, true);
            }
        }

        tree_render();
    } else if (event->type == A_I3_SYNC) {
        xcb_window_t window = event->data.data32[0];
        uint32_t rnd = event->data.data32[1];
        DLOG("[i3 sync protocol] Sending random value %d back to X11 window 0x%08x\n", rnd, window);

        void *reply = scalloc(32);
        xcb_client_message_event_t *ev = reply;

        ev->response_type = XCB_CLIENT_MESSAGE;
        ev->window = window;
        ev->type = A_I3_SYNC;
        ev->format = 32;
        ev->data.data32[0] = window;
        ev->data.data32[1] = rnd;

        xcb_send_event(conn, false, window, XCB_EVENT_MASK_NO_EVENT, (char *)ev);
        xcb_flush(conn);
        free(reply);
    } else if (event->type == A__NET_REQUEST_FRAME_EXTENTS) {
        /*
         * A client can request an estimate for the frame size which the window
         * manager will put around it before actually mapping its window. Java
         * does this (as of openjdk-7).
         *
         * Note that the calculation below is not entirely accurate — once you
         * set a different border type, it’s off. We _could_ request all the
         * window properties (which have to be set up at this point according
         * to EWMH), but that seems rather elaborate. The standard explicitly
         * says the application must cope with an estimate that is not entirely
         * accurate.
         */
        DLOG("_NET_REQUEST_FRAME_EXTENTS for window 0x%08x\n", event->window);

        /* The reply data: approximate frame size */
        Rect r = {
            config.default_border_width, /* left */
            config.default_border_width, /* right */
            config.font.height + 5,      /* top */
            config.default_border_width  /* bottom */
        };
        xcb_change_property(
            conn,
            XCB_PROP_MODE_REPLACE,
            event->window,
            A__NET_FRAME_EXTENTS,
            XCB_ATOM_CARDINAL, 32, 4,
            &r);
        xcb_flush(conn);
    } else if (event->type == A_WM_CHANGE_STATE) {
        /* http://tronche.com/gui/x/icccm/sec-4.html#s-4.1.4 */
        Con *con = con_by_window_id(event->window);

        if (con && event->data.data32[0] == 3) {
            /* this request is so we can play some animiation showing the
             * window physically moving to the tray before we close it (I
             * think) */
            DLOG("Client has requested iconic state. Closing this con. (con = %p)\n", con);
            tree_close(con, DONT_KILL_WINDOW, false, false);
            tree_render();
        } else {
            DLOG("Not handling WM_CHANGE_STATE request. (window = %d, state = %d)\n", event->window, event->data.data32[0]);
        }

    } else if (event->type == A__NET_CURRENT_DESKTOP) {
        /* This request is used by pagers and bars to change the current
         * desktop likely as a result of some user action. We interpret this as
         * a request to focus the given workspace. See
         * http://standards.freedesktop.org/wm-spec/latest/ar01s03.html#idm140251368135008
         * */
        Con *output;
        uint32_t idx = 0;
        DLOG("Request to change current desktop to index %d\n", event->data.data32[0]);

        TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
            Con *ws;
            TAILQ_FOREACH(ws, &(output_get_content(output)->nodes_head), nodes) {
                if (STARTS_WITH(ws->name, "__"))
                    continue;

                if (idx == event->data.data32[0]) {
                    /* data32[1] is a timestamp used to prevent focus race conditions */
                    if (event->data.data32[1])
                        last_timestamp = event->data.data32[1];

                    DLOG("Handling request to focus workspace %s\n", ws->name);

                    workspace_show(ws);
                    tree_render();

                    return;
                }

                ++idx;
            }
        }
    } else if (event->type == A__NET_CLOSE_WINDOW) {
        /*
         * Pagers wanting to close a window MUST send a _NET_CLOSE_WINDOW
         * client message request to the root window.
         * http://standards.freedesktop.org/wm-spec/wm-spec-latest.html#idm140200472668896
         */
        Con *con = con_by_window_id(event->window);
        if (con) {
            DLOG("Handling _NET_CLOSE_WINDOW request (con = %p)\n", con);

            if (event->data.data32[0])
                last_timestamp = event->data.data32[0];

            tree_close(con, KILL_WINDOW, false, false);
            tree_render();
        } else {
            DLOG("Couldn't find con for _NET_CLOSE_WINDOW request. (window = %d)\n", event->window);
        }
    } else {
        DLOG("unhandled clientmessage\n");
        return;
    }
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
    double aspect_ratio = 0.0;
    if ((width / height) < min_aspect) {
        aspect_ratio = min_aspect;
    } else if ((width / height) > max_aspect) {
        aspect_ratio = max_aspect;
    } else
        goto render_and_return;

    if (fabs(con->aspect_ratio - aspect_ratio) > DBL_EPSILON) {
        con->aspect_ratio = aspect_ratio;
        changed = true;
    }

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

    bool urgency_hint;
    if (reply == NULL)
        reply = xcb_get_property_reply(conn, xcb_icccm_get_wm_hints(conn, window), NULL);
    window_update_hints(con->window, reply, &urgency_hint);
    con_set_urgency(con, urgency_hint);
    tree_render();

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
                                                                       false, window, XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, 0, 32),
                                      NULL);
        if (prop == NULL)
            return false;
    }

    window_update_transient_for(con->window, prop);

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
                                                                       false, window, A_WM_CLIENT_LEADER, XCB_ATOM_WINDOW, 0, 32),
                                      NULL);
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
static void handle_focus_in(xcb_focus_in_event_t *event) {
    DLOG("focus change in, for window 0x%08x\n", event->event);
    Con *con;
    if ((con = con_by_window_id(event->event)) == NULL || con->window == NULL)
        return;
    DLOG("That is con %p / %s\n", con, con->name);

    if (event->mode == XCB_NOTIFY_MODE_GRAB ||
        event->mode == XCB_NOTIFY_MODE_UNGRAB) {
        DLOG("FocusIn event for grab/ungrab, ignoring\n");
        return;
    }

    if (event->detail == XCB_NOTIFY_DETAIL_POINTER) {
        DLOG("notify detail is pointer, ignoring this event\n");
        return;
    }

    if (focused_id == event->event) {
        DLOG("focus matches the currently focused window, not doing anything\n");
        return;
    }

    /* Skip dock clients, they cannot get the i3 focus. */
    if (con->parent->type == CT_DOCKAREA) {
        DLOG("This is a dock client, not focusing.\n");
        return;
    }

    DLOG("focus is different, updating decorations\n");

    /* Get the currently focused workspace to check if the focus change also
     * involves changing workspaces. If so, we need to call workspace_show() to
     * correctly update state and send the IPC event. */
    Con *ws = con_get_workspace(con);
    if (ws != con_get_workspace(focused))
        workspace_show(ws);

    con_focus(con);
    /* We update focused_id because we don’t need to set focus again */
    focused_id = event->event;
    x_push_changes(croot);
    return;
}

/*
 * Handles the WM_CLASS property for assignments and criteria selection.
 *
 */
static bool handle_class_change(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                                xcb_atom_t name, xcb_get_property_reply_t *prop) {
    Con *con;
    if ((con = con_by_window_id(window)) == NULL || con->window == NULL)
        return false;

    if (prop == NULL) {
        prop = xcb_get_property_reply(conn, xcb_get_property_unchecked(conn,
                                                                       false, window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 0, 32),
                                      NULL);

        if (prop == NULL)
            return false;
    }

    window_update_class(con->window, prop, false);

    return true;
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
    {0, 128, handle_windowname_change},
    {0, UINT_MAX, handle_hints},
    {0, 128, handle_windowname_change_legacy},
    {0, UINT_MAX, handle_normal_hints},
    {0, UINT_MAX, handle_clientleader_change},
    {0, UINT_MAX, handle_transient_for},
    {0, 128, handle_windowrole_change},
    {0, 128, handle_class_change}};
#define NUM_HANDLERS (sizeof(property_handlers) / sizeof(struct property_handler_t))

/*
 * Sets the appropriate atoms for the property handlers after the atoms were
 * received from X11
 *
 */
void property_handlers_init(void) {
    sn_monitor_context_new(sndisplay, conn_screen, startup_monitor_event, NULL, NULL);

    property_handlers[0].atom = A__NET_WM_NAME;
    property_handlers[1].atom = XCB_ATOM_WM_HINTS;
    property_handlers[2].atom = XCB_ATOM_WM_NAME;
    property_handlers[3].atom = XCB_ATOM_WM_NORMAL_HINTS;
    property_handlers[4].atom = A_WM_CLIENT_LEADER;
    property_handlers[5].atom = XCB_ATOM_WM_TRANSIENT_FOR;
    property_handlers[6].atom = A_WM_WINDOW_ROLE;
    property_handlers[7].atom = XCB_ATOM_WM_CLASS;
}

static void property_notify(uint8_t state, xcb_window_t window, xcb_atom_t atom) {
    struct property_handler_t *handler = NULL;
    xcb_get_property_reply_t *propr = NULL;

    for (size_t c = 0; c < sizeof(property_handlers) / sizeof(struct property_handler_t); c++) {
        if (property_handlers[c].atom != atom)
            continue;

        handler = &property_handlers[c];
        break;
    }

    if (handler == NULL) {
        //DLOG("Unhandled property notify for atom %d (0x%08x)\n", atom, atom);
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
    DLOG("event type %d, xkb_base %d\n", type, xkb_base);
    if (randr_base > -1 &&
        type == randr_base + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
        handle_screen_change(event);
        return;
    }

    if (xkb_base > -1 && type == xkb_base) {
        DLOG("xkb event, need to handle it.\n");

        xcb_xkb_state_notify_event_t *state = (xcb_xkb_state_notify_event_t *)event;
        if (state->xkbType == XCB_XKB_MAP_NOTIFY) {
            if (event_is_ignored(event->sequence, type)) {
                DLOG("Ignoring map notify event for sequence %d.\n", state->sequence);
            } else {
                DLOG("xkb map notify, sequence %d, time %d\n", state->sequence, state->time);
                add_ignore_event(event->sequence, type);
                ungrab_all_keys(conn);
                translate_keysyms();
                grab_all_keys(conn, false);
            }
        } else if (state->xkbType == XCB_XKB_STATE_NOTIFY) {
            DLOG("xkb state group = %d\n", state->group);

            /* See The XKB Extension: Library Specification, section 14.1 */
            /* We check if the current group (each group contains
             * two levels) has been changed. Mode_switch activates
             * group XkbGroup2Index */
            if (xkb_current_group == state->group)
                return;
            xkb_current_group = state->group;
            if (state->group == XCB_XKB_GROUP_1) {
                DLOG("Mode_switch disabled\n");
                ungrab_all_keys(conn);
                grab_all_keys(conn, false);
            } else {
                DLOG("Mode_switch enabled\n");
                grab_all_keys(conn, false);
            }
        }

        return;
    }

    switch (type) {
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE:
            handle_key_press((xcb_key_press_event_t *)event);
            break;

        case XCB_BUTTON_PRESS:
            handle_button_press((xcb_button_press_event_t *)event);
            break;

        case XCB_MAP_REQUEST:
            handle_map_request((xcb_map_request_event_t *)event);
            break;

        case XCB_UNMAP_NOTIFY:
            handle_unmap_notify_event((xcb_unmap_notify_event_t *)event);
            break;

        case XCB_DESTROY_NOTIFY:
            handle_destroy_notify_event((xcb_destroy_notify_event_t *)event);
            break;

        case XCB_EXPOSE:
            handle_expose_event((xcb_expose_event_t *)event);
            break;

        case XCB_MOTION_NOTIFY:
            handle_motion_notify((xcb_motion_notify_event_t *)event);
            break;

        /* Enter window = user moved his mouse over the window */
        case XCB_ENTER_NOTIFY:
            handle_enter_notify((xcb_enter_notify_event_t *)event);
            break;

        /* Client message are sent to the root window. The only interesting
         * client message for us is _NET_WM_STATE, we honour
         * _NET_WM_STATE_FULLSCREEN and _NET_WM_STATE_DEMANDS_ATTENTION */
        case XCB_CLIENT_MESSAGE:
            handle_client_message((xcb_client_message_event_t *)event);
            break;

        /* Configure request = window tried to change size on its own */
        case XCB_CONFIGURE_REQUEST:
            handle_configure_request((xcb_configure_request_event_t *)event);
            break;

        /* Mapping notify = keyboard mapping changed (Xmodmap), re-grab bindings */
        case XCB_MAPPING_NOTIFY:
            handle_mapping_notify((xcb_mapping_notify_event_t *)event);
            break;

        case XCB_FOCUS_IN:
            handle_focus_in((xcb_focus_in_event_t *)event);
            break;

        case XCB_PROPERTY_NOTIFY: {
            xcb_property_notify_event_t *e = (xcb_property_notify_event_t *)event;
            last_timestamp = e->time;
            property_notify(e->state, e->window, e->atom);
            break;
        }

        default:
            //DLOG("Unhandled event of type %d\n", type);
            break;
    }
}
