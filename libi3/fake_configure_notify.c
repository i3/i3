/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "libi3.h"

#include <stdlib.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>

/*
 * Generates a configure_notify event and sends it to the given window
 * Applications need this to think they’ve configured themselves correctly.
 * The truth is, however, that we will manage them.
 *
 */
void fake_configure_notify(xcb_connection_t *conn, xcb_rectangle_t r, xcb_window_t window, int border_width) {
    /* Every X11 event is 32 bytes long. Therefore, XCB will copy 32 bytes.
     * In order to properly initialize these bytes, we allocate 32 bytes even
     * though we only need less for an xcb_configure_notify_event_t */
    void *event = scalloc(32, 1);
    xcb_configure_notify_event_t *generated_event = event;

    generated_event->event = window;
    generated_event->window = window;
    generated_event->response_type = XCB_CONFIGURE_NOTIFY;

    generated_event->x = r.x;
    generated_event->y = r.y;
    generated_event->width = r.width;
    generated_event->height = r.height;

    generated_event->border_width = border_width;
    generated_event->above_sibling = XCB_NONE;
    generated_event->override_redirect = false;

    xcb_send_event(conn, false, window, XCB_EVENT_MASK_STRUCTURE_NOTIFY, (char *)generated_event);
    xcb_flush(conn);

    free(event);
}
