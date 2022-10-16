/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * drag.c: click and drag.
 *
 */
#include "all.h"

/* Custom data structure used to track dragging-related events. */
struct drag_x11_cb {
    ev_prepare prepare;

    /* Whether this modal event loop should be exited and with which result. */
    drag_result_t result;

    /* The container that is being dragged or resized, or NULL if this is a
     * drag of the resize handle. */
    Con *con;

    /* The original event that initiated the drag. */
    const xcb_button_press_event_t *event;

    /* The dimensions of con when the loop was started. */
    Rect old_rect;

    /* The callback to invoke after every pointer movement. */
    callback_t callback;

    /* Drag distance threshold exceeded. If use_threshold is not set, then
     * threshold_exceeded is always true. */
    bool threshold_exceeded;

    /* Cursor to set after the threshold is exceeded. */
    xcb_cursor_t xcursor;

    /* User data pointer for callback. */
    const void *extra;
};

static bool threshold_exceeded(uint32_t x1, uint32_t y1,
                               uint32_t x2, uint32_t y2) {
    /* The threshold is about the height of one window decoration. */
    const uint32_t threshold = logical_px(15);
    return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2) > threshold * threshold;
}

static bool drain_drag_events(EV_P, struct drag_x11_cb *dragloop) {
    xcb_motion_notify_event_t *last_motion_notify = NULL;
    xcb_generic_event_t *event;

    while ((event = xcb_poll_for_event(conn)) != NULL) {
        if (event->response_type == 0) {
            xcb_generic_error_t *error = (xcb_generic_error_t *)event;
            DLOG("X11 Error received (probably harmless)! sequence 0x%x, error_code = %d\n",
                 error->sequence, error->error_code);
            free(event);
            continue;
        }

        /* Strip off the highest bit (set if the event is generated) */
        int type = (event->response_type & 0x7F);

        switch (type) {
            case XCB_BUTTON_RELEASE:
                dragloop->result = DRAG_SUCCESS;
                break;

            case XCB_KEY_PRESS:
                DLOG("A key was pressed during drag, reverting changes.\n");
                dragloop->result = DRAG_REVERT;
                handle_event(type, event);
                break;

            case XCB_UNMAP_NOTIFY: {
                xcb_unmap_notify_event_t *unmap_event = (xcb_unmap_notify_event_t *)event;
                Con *con = con_by_window_id(unmap_event->window);

                if (con != NULL) {
                    DLOG("UnmapNotify for window 0x%08x (container %p)\n", unmap_event->window, con);

                    if (con_get_workspace(con) == con_get_workspace(focused)) {
                        DLOG("UnmapNotify for a managed window on the current workspace, aborting\n");
                        dragloop->result = DRAG_ABORT;
                    }
                }

                handle_event(type, event);
                break;
            }

            case XCB_MOTION_NOTIFY:
                /* motion_notify events are saved for later */
                FREE(last_motion_notify);
                last_motion_notify = (xcb_motion_notify_event_t *)event;
                break;

            default:
                DLOG("Passing to original handler\n");
                handle_event(type, event);
                break;
        }

        if (last_motion_notify != (xcb_motion_notify_event_t *)event)
            free(event);

        if (dragloop->result != DRAGGING) {
            ev_break(EV_A_ EVBREAK_ONE);
            if (dragloop->result == DRAG_SUCCESS) {
                /* Ensure motion notify events are handled. */
                break;
            } else {
                free(last_motion_notify);
                return true;
            }
        }
    }

    if (last_motion_notify == NULL) {
        return true;
    }

    if (!dragloop->threshold_exceeded &&
        threshold_exceeded(last_motion_notify->root_x, last_motion_notify->root_y,
                           dragloop->event->root_x, dragloop->event->root_y)) {
        if (dragloop->xcursor != XCB_NONE) {
            xcb_change_active_pointer_grab(
                conn,
                dragloop->xcursor,
                XCB_CURRENT_TIME,
                XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION);
        }
        dragloop->threshold_exceeded = true;
    }

    /* Ensure that we are either dragging the resize handle (con is NULL) or that the
     * container still exists. The latter might not be true, e.g., if the window closed
     * for any reason while the user was dragging it. */
    if (dragloop->threshold_exceeded && (!dragloop->con || con_exists(dragloop->con))) {
        dragloop->callback(
            dragloop->con,
            &(dragloop->old_rect),
            last_motion_notify->root_x,
            last_motion_notify->root_y,
            dragloop->event,
            dragloop->extra);
    }
    FREE(last_motion_notify);

    xcb_flush(conn);
    return dragloop->result != DRAGGING;
}

static void xcb_drag_prepare_cb(EV_P_ ev_prepare *w, int revents) {
    struct drag_x11_cb *dragloop = (struct drag_x11_cb *)w->data;
    while (!drain_drag_events(EV_A, dragloop)) {
        /* repeatedly drain events: draining might produce additional ones */
    }
}

/*
 * This function grabs your pointer and keyboard and lets you drag stuff around
 * (borders). Every time you move your mouse, an XCB_MOTION_NOTIFY event will
 * be received and the given callback will be called with the parameters
 * specified (client, the original event), the original rect of the client,
 * and the new coordinates (x, y).
 *
 * If use_threshold is set, dragging only starts after the user moves the
 * pointer past a certain threshold. That is, the cursor will not be set and the
 * callback will not be called until then.
 *
 */
drag_result_t drag_pointer(Con *con, const xcb_button_press_event_t *event,
                           xcb_window_t confine_to, int cursor,
                           bool use_threshold, callback_t callback,
                           const void *extra) {
    xcb_cursor_t xcursor = cursor ? xcursor_get_cursor(cursor) : XCB_NONE;

    /* Grab the pointer */
    xcb_grab_pointer_cookie_t cookie;
    xcb_grab_pointer_reply_t *reply;
    xcb_generic_error_t *error;

    cookie = xcb_grab_pointer(conn,
                              false,                                                         /* get all pointer events specified by the following mask */
                              root,                                                          /* grab the root window */
                              XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION, /* which events to let through */
                              XCB_GRAB_MODE_ASYNC,                                           /* pointer events should continue as normal */
                              XCB_GRAB_MODE_ASYNC,                                           /* keyboard mode */
                              confine_to,                                                    /* confine_to = in which window should the cursor stay */
                              use_threshold ? XCB_NONE : xcursor,                            /* possibly display a special cursor */
                              XCB_CURRENT_TIME);

    if ((reply = xcb_grab_pointer_reply(conn, cookie, &error)) == NULL) {
        ELOG("Could not grab pointer (error_code = %d)\n", error->error_code);
        free(error);
        return DRAG_ABORT;
    }

    free(reply);

    /* Grab the keyboard */
    xcb_grab_keyboard_cookie_t keyb_cookie;
    xcb_grab_keyboard_reply_t *keyb_reply;

    keyb_cookie = xcb_grab_keyboard(conn,
                                    false, /* get all keyboard events */
                                    root,  /* grab the root window */
                                    XCB_CURRENT_TIME,
                                    XCB_GRAB_MODE_ASYNC, /* continue processing pointer events as normal */
                                    XCB_GRAB_MODE_ASYNC  /* keyboard mode */
    );

    if ((keyb_reply = xcb_grab_keyboard_reply(conn, keyb_cookie, &error)) == NULL) {
        ELOG("Could not grab keyboard (error_code = %d)\n", error->error_code);
        free(error);
        xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
        return DRAG_ABORT;
    }

    free(keyb_reply);

    /* Go into our own event loop */
    struct drag_x11_cb loop = {
        .result = DRAGGING,
        .con = con,
        .event = event,
        .callback = callback,
        .threshold_exceeded = !use_threshold,
        .xcursor = xcursor,
        .extra = extra,
    };
    ev_prepare *prepare = &loop.prepare;
    if (con)
        loop.old_rect = con->rect;
    ev_prepare_init(prepare, xcb_drag_prepare_cb);
    prepare->data = &loop;
    main_set_x11_cb(false);
    ev_prepare_start(main_loop, prepare);

    ev_loop(main_loop, 0);

    ev_prepare_stop(main_loop, prepare);
    main_set_x11_cb(true);

    xcb_ungrab_keyboard(conn, XCB_CURRENT_TIME);
    xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
    xcb_flush(conn);

    return loop.result;
}
