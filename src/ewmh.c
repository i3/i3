/*
 * vim:ts=4:sw=4:expandtab
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

#include "all.h"

/*
 * Updates _NET_CURRENT_DESKTOP with the current desktop number.
 *
 * EWMH: The index of the current desktop. This is always an integer between 0
 * and _NET_NUMBER_OF_DESKTOPS - 1.
 *
 */
void ewmh_update_current_desktop() {
    Con *focused_ws = con_get_workspace(focused);
    Con *output;
    uint32_t idx = 0;
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        Con *ws;
        TAILQ_FOREACH(ws, &(output_get_content(output)->nodes_head), nodes) {
            if (ws == focused_ws) {
                xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root,
                        atoms[_NET_CURRENT_DESKTOP], CARDINAL, 32, 1, &idx);
                return;
            }
            ++idx;
        }
    }
}

/*
 * Updates _NET_ACTIVE_WINDOW with the currently focused window.
 *
 * EWMH: The window ID of the currently active window or None if no window has
 * the focus.
 *
 */
void ewmh_update_active_window(xcb_window_t window) {
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root,
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
    int num_workspaces = 0, count = 0;
    Rect last_rect = {0, 0, 0, 0};
    Con *output;

    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        Con *ws;
        TAILQ_FOREACH(ws, &(output_get_content(output)->nodes_head), nodes) {
            /* Check if we need to initialize last_rect. The case that the
             * first workspace is all-zero may happen when the user
             * assigned workspace 2 for his first screen, for example. Thus
             * we need an initialized last_rect in the very first run of
             * the following loop. */
            if (last_rect.width == 0 && last_rect.height == 0 &&
                    ws->rect.width != 0 && ws->rect.height != 0) {
                memcpy(&last_rect, &(ws->rect), sizeof(Rect));
            }
            num_workspaces++;
        }
    }

    DLOG("Got %d workspaces\n", num_workspaces);
    uint8_t *workarea = smalloc(sizeof(Rect) * num_workspaces);
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        Con *ws;
        TAILQ_FOREACH(ws, &(output_get_content(output)->nodes_head), nodes) {
            DLOG("storing %d: %dx%d with %d x %d\n", count, ws->rect.x,
                 ws->rect.y, ws->rect.width, ws->rect.height);
            /* If a workspace is not yet initialized and thus its
             * dimensions are zero, we will instead put the dimensions
             * of the last workspace in the list. For example firefox
             * intersects all workspaces and does not cope so well with
             * an all-zero workspace. */
            if (ws->rect.width == 0 || ws->rect.height == 0) {
                DLOG("re-using last_rect (%dx%d, %d, %d)\n",
                     last_rect.x, last_rect.y, last_rect.width,
                     last_rect.height);
                memcpy(workarea + (sizeof(Rect) * count++), &last_rect, sizeof(Rect));
                continue;
            }
            memcpy(workarea + (sizeof(Rect) * count++), &(ws->rect), sizeof(Rect));
            memcpy(&last_rect, &(ws->rect), sizeof(Rect));
        }
    }
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root,
            atoms[_NET_WORKAREA], CARDINAL, 32,
            num_workspaces * (sizeof(Rect) / sizeof(uint32_t)),
            workarea);
    free(workarea);
    xcb_flush(conn);
}
