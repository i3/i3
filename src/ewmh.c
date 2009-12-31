/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * ewmh.c: Functions to get/set certain EWMH properties easily.
 *
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "data.h"
#include "table.h"
#include "i3.h"
#include "xcb.h"
#include "util.h"
#include "log.h"

/*
 * Updates _NET_CURRENT_DESKTOP with the current desktop number.
 *
 * EWMH: The index of the current desktop. This is always an integer between 0
 * and _NET_NUMBER_OF_DESKTOPS - 1.
 *
 */
void ewmh_update_current_desktop() {
        uint32_t current_desktop = c_ws->num;
        xcb_change_property(global_conn, XCB_PROP_MODE_REPLACE, root,
                            atoms[_NET_CURRENT_DESKTOP], CARDINAL, 32, 1,
                            &current_desktop);
}

/*
 * Updates _NET_ACTIVE_WINDOW with the currently focused window.
 *
 * EWMH: The window ID of the currently active window or None if no window has
 * the focus.
 *
 */
void ewmh_update_active_window(xcb_window_t window) {
        xcb_change_property(global_conn, XCB_PROP_MODE_REPLACE, root,
                            atoms[_NET_ACTIVE_WINDOW], WINDOW, 32, 1, &window);
}

/*
 * Updates the workarea for each desktop.
 *
 * EWMH: Contains a geometry for each desktop. These geometries specify an area
 * that is completely contained within the viewport. Work area SHOULD be used by
 * desktop applications to place desktop icons appropriately.
 *
 */
void ewmh_update_workarea() {
        Workspace *ws;
        int num_workspaces = 0, count = 0;
        /* Get the number of workspaces */
        TAILQ_FOREACH(ws, workspaces, workspaces)
                num_workspaces++;
        DLOG("Got %d workspaces\n", num_workspaces);
        uint8_t *workarea = smalloc(sizeof(Rect) * num_workspaces);
        TAILQ_FOREACH(ws, workspaces, workspaces) {
                DLOG("storing %d: %dx%d with %d x %d\n", count, ws->rect.x, ws->rect.y, ws->rect.width, ws->rect.height);
                memcpy(workarea + (sizeof(Rect) * count++), &(ws->rect), sizeof(Rect));
        }
        xcb_change_property(global_conn, XCB_PROP_MODE_REPLACE, root,
                            atoms[_NET_WORKAREA], CARDINAL, 32,
                            num_workspaces * (sizeof(Rect) / sizeof(uint32_t)),
                            workarea);
        free(workarea);
        xcb_flush(global_conn);
}
