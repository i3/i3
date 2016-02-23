#undef I3__FILE__
#define I3__FILE__ "ewmh.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * ewmh.c: Get/set certain EWMH properties easily.
 *
 */
#include "all.h"

xcb_window_t ewmh_window;

/*
 * Updates _NET_CURRENT_DESKTOP with the current desktop number.
 *
 * EWMH: The index of the current desktop. This is always an integer between 0
 * and _NET_NUMBER_OF_DESKTOPS - 1.
 *
 */
void ewmh_update_current_desktop(void) {
    const uint32_t idx = ewmh_get_workspace_index(focused);
    if (idx != NET_WM_DESKTOP_NONE) {
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A__NET_CURRENT_DESKTOP, XCB_ATOM_CARDINAL, 32, 1, &idx);
    }
}

/*
 * Updates _NET_NUMBER_OF_DESKTOPS which we interpret as the number of
 * noninternal workspaces.
 */
void ewmh_update_number_of_desktops(void) {
    Con *output;
    uint32_t idx = 0;

    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        Con *ws;
        TAILQ_FOREACH(ws, &(output_get_content(output)->nodes_head), nodes) {
            if (STARTS_WITH(ws->name, "__"))
                continue;
            ++idx;
        }
    }

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root,
                        A__NET_NUMBER_OF_DESKTOPS, XCB_ATOM_CARDINAL, 32, 1, &idx);
}

/*
 * Updates _NET_DESKTOP_NAMES: "The names of all virtual desktops. This is a
 * list of NULL-terminated strings in UTF-8 encoding"
 */
void ewmh_update_desktop_names(void) {
    Con *output;
    int msg_length = 0;

    /* count the size of the property message to set */
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        Con *ws;
        TAILQ_FOREACH(ws, &(output_get_content(output)->nodes_head), nodes) {
            if (STARTS_WITH(ws->name, "__"))
                continue;
            msg_length += strlen(ws->name) + 1;
        }
    }

    char desktop_names[msg_length];
    int current_position = 0;

    /* fill the buffer with the names of the i3 workspaces */
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        Con *ws;
        TAILQ_FOREACH(ws, &(output_get_content(output)->nodes_head), nodes) {
            if (STARTS_WITH(ws->name, "__"))
                continue;

            for (size_t i = 0; i < strlen(ws->name) + 1; i++) {
                desktop_names[current_position++] = ws->name[i];
            }
        }
    }

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root,
                        A__NET_DESKTOP_NAMES, A_UTF8_STRING, 8, msg_length, desktop_names);
}

/*
 * Updates _NET_DESKTOP_VIEWPORT, which is an array of pairs of cardinals that
 * define the top left corner of each desktop's viewport.
 */
void ewmh_update_desktop_viewport(void) {
    Con *output;
    int num_desktops = 0;
    /* count number of desktops */
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        Con *ws;
        TAILQ_FOREACH(ws, &(output_get_content(output)->nodes_head), nodes) {
            if (STARTS_WITH(ws->name, "__"))
                continue;

            num_desktops++;
        }
    }

    uint32_t viewports[num_desktops * 2];

    int current_position = 0;
    /* fill the viewport buffer */
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        Con *ws;
        TAILQ_FOREACH(ws, &(output_get_content(output)->nodes_head), nodes) {
            if (STARTS_WITH(ws->name, "__"))
                continue;

            viewports[current_position++] = output->rect.x;
            viewports[current_position++] = output->rect.y;
        }
    }

    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root,
                        A__NET_DESKTOP_VIEWPORT, XCB_ATOM_CARDINAL, 32, current_position, &viewports);
}

static void ewmh_update_wm_desktop_recursively(Con *con, const uint32_t desktop) {
    /* Recursively call this to descend through the entire subtree. */
    Con *child;
    TAILQ_FOREACH(child, &(con->nodes_head), nodes) {
        ewmh_update_wm_desktop_recursively(child, desktop);
    }
    /* If con is a workspace, we also need to go through the floating windows on it. */
    if (con->type == CT_WORKSPACE) {
        TAILQ_FOREACH(child, &(con->floating_head), floating_windows) {
            ewmh_update_wm_desktop_recursively(child, desktop);
        }
    }

    if (!con_has_managed_window(con))
        return;

    const xcb_window_t window = con->window->id;

    uint32_t wm_desktop = desktop;
    /* Sticky windows are only actually sticky when they are floating or inside
     * a floating container. This is technically still slightly wrong, since
     * sticky windows will only be on all workspaces on this output, but we
     * ignore multi-monitor situations for this since the spec isn't too
     * precise on this anyway. */
    if (con_is_sticky(con) && con_is_floating(con)) {
        wm_desktop = NET_WM_DESKTOP_ALL;
    }

    /* If this is the cached value, we don't need to do anything. */
    if (con->window->wm_desktop == wm_desktop)
        return;
    con->window->wm_desktop = wm_desktop;

    if (wm_desktop != NET_WM_DESKTOP_NONE) {
        DLOG("Setting _NET_WM_DESKTOP = %d for window 0x%08x.\n", wm_desktop, window);
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, window, A__NET_WM_DESKTOP, XCB_ATOM_CARDINAL, 32, 1, &wm_desktop);
    } else {
        /* If we can't determine the workspace index, delete the property. We'd
         * rather not set it than lie. */
        ELOG("Failed to determine the proper EWMH desktop index for window 0x%08x, deleting _NET_WM_DESKTOP.\n", window);
        xcb_delete_property(conn, window, A__NET_WM_DESKTOP);
    }
}

/*
 * Updates _NET_WM_DESKTOP for all windows.
 * A request will only be made if the cached value differs from the calculated value.
 *
 */
void ewmh_update_wm_desktop(void) {
    uint32_t desktop = 0;

    Con *output;
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        Con *workspace;
        TAILQ_FOREACH(workspace, &(output_get_content(output)->nodes_head), nodes) {
            if (con_is_internal(workspace))
                continue;

            ewmh_update_wm_desktop_recursively(workspace, desktop);
            ++desktop;
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
 * Updates _NET_WM_VISIBLE_NAME.
 *
 */
void ewmh_update_visible_name(xcb_window_t window, const char *name) {
    if (name != NULL) {
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, window, A__NET_WM_VISIBLE_NAME, A_UTF8_STRING, 8, strlen(name), name);
    } else {
        xcb_delete_property(conn, window, A__NET_WM_VISIBLE_NAME);
    }
}

/*
 * i3 currently does not support _NET_WORKAREA, because it does not correspond
 * to i3’s concept of workspaces. See also:
 * http://bugs.i3wm.org/539
 * http://bugs.i3wm.org/301
 * http://bugs.i3wm.org/1038
 *
 * We need to actively delete this property because some display managers (e.g.
 * LightDM) set it.
 *
 * EWMH: Contains a geometry for each desktop. These geometries specify an area
 * that is completely contained within the viewport. Work area SHOULD be used by
 * desktop applications to place desktop icons appropriately.
 *
 */
void ewmh_update_workarea(void) {
    xcb_delete_property(conn, root, A__NET_WORKAREA);
}

/*
 * Updates the _NET_CLIENT_LIST hint.
 *
 */
void ewmh_update_client_list(xcb_window_t *list, int num_windows) {
    xcb_change_property(
        conn,
        XCB_PROP_MODE_REPLACE,
        root,
        A__NET_CLIENT_LIST,
        XCB_ATOM_WINDOW,
        32,
        num_windows,
        list);
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
 * Set or remove _NET_WM_STATE_STICKY on the window.
 *
 */
void ewmh_update_sticky(xcb_window_t window, bool sticky) {
    if (sticky) {
        DLOG("Setting _NET_WM_STATE_STICKY for window = %d.\n", window);
        xcb_add_property_atom(conn, window, A__NET_WM_STATE, A__NET_WM_STATE_STICKY);
    } else {
        DLOG("Removing _NET_WM_STATE_STICKY for window = %d.\n", window);
        xcb_remove_property_atom(conn, window, A__NET_WM_STATE, A__NET_WM_STATE_STICKY);
    }
}

/*
 * Set up the EWMH hints on the root window.
 *
 */
void ewmh_setup_hints(void) {
    xcb_atom_t supported_atoms[] = {
#define xmacro(atom) A_##atom,
#include "atoms.xmacro"
#undef xmacro
    };

    /* Set up the window manager’s name. According to EWMH, section "Root Window
     * Properties", to indicate that an EWMH-compliant window manager is
     * present, a child window has to be created (and kept alive as long as the
     * window manager is running) which has the _NET_SUPPORTING_WM_CHECK and
     * _NET_WM_ATOMS. */
    ewmh_window = xcb_generate_id(conn);
    /* We create the window and put it at (-1, -1) so that it is off-screen. */
    xcb_create_window(
        conn,
        XCB_COPY_FROM_PARENT,        /* depth */
        ewmh_window,                 /* window id */
        root,                        /* parent */
        -1, -1, 1, 1,                /* dimensions (x, y, w, h) */
        0,                           /* border */
        XCB_WINDOW_CLASS_INPUT_ONLY, /* window class */
        XCB_COPY_FROM_PARENT,        /* visual */
        XCB_CW_OVERRIDE_REDIRECT,
        (uint32_t[]){1});
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, ewmh_window, A__NET_SUPPORTING_WM_CHECK, XCB_ATOM_WINDOW, 32, 1, &ewmh_window);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, ewmh_window, A__NET_WM_NAME, A_UTF8_STRING, 8, strlen("i3"), "i3");
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A__NET_SUPPORTING_WM_CHECK, XCB_ATOM_WINDOW, 32, 1, &ewmh_window);

    /* I’m not entirely sure if we need to keep _NET_WM_NAME on root. */
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A__NET_WM_NAME, A_UTF8_STRING, 8, strlen("i3"), "i3");

    /* only send the first 33 atoms (last one is _NET_CLOSE_WINDOW) increment that number when adding supported atoms */
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, root, A__NET_SUPPORTED, XCB_ATOM_ATOM, 32, /* number of atoms */ 33, supported_atoms);

    /* We need to map this window to be able to set the input focus to it if no other window is available to be focused. */
    xcb_map_window(conn, ewmh_window);
    xcb_configure_window(conn, ewmh_window, XCB_CONFIG_WINDOW_STACK_MODE, (uint32_t[]){XCB_STACK_MODE_BELOW});
}

/*
 * Returns the workspace container as enumerated by the EWMH desktop model.
 * Returns NULL if no workspace could be found for the index.
 *
 * This is the reverse of ewmh_get_workspace_index.
 *
 */
Con *ewmh_get_workspace_by_index(uint32_t idx) {
    if (idx == NET_WM_DESKTOP_NONE)
        return NULL;

    uint32_t current_index = 0;

    Con *output;
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        Con *workspace;
        TAILQ_FOREACH(workspace, &(output_get_content(output)->nodes_head), nodes) {
            if (con_is_internal(workspace))
                continue;

            if (current_index == idx)
                return workspace;

            ++current_index;
        }
    }

    return NULL;
}

/*
 * Returns the EWMH desktop index for the workspace the given container is on.
 * Returns NET_WM_DESKTOP_NONE if the desktop index cannot be determined.
 *
 * This is the reverse of ewmh_get_workspace_by_index.
 *
 */
uint32_t ewmh_get_workspace_index(Con *con) {
    uint32_t index = 0;

    Con *workspace = con_get_workspace(con);
    Con *output;
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        Con *current;
        TAILQ_FOREACH(current, &(output_get_content(output)->nodes_head), nodes) {
            if (con_is_internal(current))
                continue;

            if (current == workspace)
                return index;

            ++index;
        }
    }

    return NET_WM_DESKTOP_NONE;
}
