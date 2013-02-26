#undef I3__FILE__
#define I3__FILE__ "ewmh.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * ewmh.c: Get/set certain EWMH properties easily.
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
void ewmh_update_current_desktop(void) {
    Con *focused_ws = con_get_workspace(focused);
    Con *output;
    uint32_t idx = 0;
    /* We count to get the index of this workspace because named workspaces
     * don’t have the ->num property */
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        Con *ws;
        TAILQ_FOREACH(ws, &(output_get_content(output)->nodes_head), nodes) {
            if (ws == focused_ws) {
                xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root,
                        A__NET_CURRENT_DESKTOP, XCB_ATOM_CARDINAL, 32, 1, &idx);
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
            A__NET_ACTIVE_WINDOW, XCB_ATOM_WINDOW, 32, 1, &window);
}

/*
 * Updates the workarea for each desktop.
 *
 * This function is not called at the moment due to:
 * http://bugs.i3wm.org/539
 * http://bugs.i3wm.org/301
 *
 * EWMH: Contains a geometry for each desktop. These geometries specify an area
 * that is completely contained within the viewport. Work area SHOULD be used by
 * desktop applications to place desktop icons appropriately.
 *
 */
void ewmh_update_workarea(void) {
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
            A__NET_WORKAREA, XCB_ATOM_CARDINAL, 32,
            num_workspaces * (sizeof(Rect) / sizeof(uint32_t)),
            workarea);
    free(workarea);
    xcb_flush(conn);
}

/*
 * Updates the _NET_CLIENT_LIST_STACKING hint.
 *
 */
void ewmh_update_client_list_stacking(xcb_window_t *stack, int num_windows) {
    xcb_change_property(
        conn,
        XCB_PROP_MODE_REPLACE,
        root,
        A__NET_CLIENT_LIST_STACKING,
        XCB_ATOM_WINDOW,
        32,
        num_windows,
        stack);
}

/*
 * Set up the EWMH hints on the root window.
 *
 */
void ewmh_setup_hints(void) {
    xcb_atom_t supported_atoms[] = {
#define xmacro(atom) A_ ## atom,
#include "atoms.xmacro"
#undef xmacro
    };

    /* Set up the window manager’s name. According to EWMH, section "Root Window
     * Properties", to indicate that an EWMH-compliant window manager is
     * present, a child window has to be created (and kept alive as long as the
     * window manager is running) which has the _NET_SUPPORTING_WM_CHECK and
     * _NET_WM_ATOMS. */
    xcb_window_t child_window = xcb_generate_id(conn);
    xcb_create_window(
        conn,
        XCB_COPY_FROM_PARENT, /* depth */
        child_window, /* window id */
        root, /* parent */
        0, 0, 1, 1, /* dimensions (x, y, w, h) */
        0, /* border */
        XCB_WINDOW_CLASS_INPUT_ONLY, /* window class */
        XCB_COPY_FROM_PARENT, /* visual */
        0,
        NULL);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, child_window, A__NET_SUPPORTING_WM_CHECK, XCB_ATOM_WINDOW, 32, 1, &child_window);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, child_window, A__NET_WM_NAME, A_UTF8_STRING, 8, strlen("i3"), "i3");
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A__NET_SUPPORTING_WM_CHECK, XCB_ATOM_WINDOW, 32, 1, &child_window);

    /* I’m not entirely sure if we need to keep _NET_WM_NAME on root. */
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A__NET_WM_NAME, A_UTF8_STRING, 8, strlen("i3"), "i3");

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A__NET_SUPPORTED, XCB_ATOM_ATOM, 32, 19, supported_atoms);
}
