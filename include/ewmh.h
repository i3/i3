/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * ewmh.c: Get/set certain EWMH properties easily.
 *
 */
#pragma once

#include <config.h>

/**
 * Updates all the EWMH desktop properties.
 *
 */
void ewmh_update_desktop_properties(void);

/**
 * Updates _NET_CURRENT_DESKTOP with the current desktop number.
 *
 * EWMH: The index of the current desktop. This is always an integer between 0
 * and _NET_NUMBER_OF_DESKTOPS - 1.
 *
 */
void ewmh_update_current_desktop(void);

/**
 * Updates _NET_WM_DESKTOP for all windows.
 * A request will only be made if the cached value differs from the calculated value.
 *
 */
void ewmh_update_wm_desktop(void);

/**
 * Updates _NET_ACTIVE_WINDOW with the currently focused window.
 *
 * EWMH: The window ID of the currently active window or None if no window has
 * the focus.
 *
 */
void ewmh_update_active_window(xcb_window_t window);

/**
 * Updates _NET_WM_VISIBLE_NAME.
 *
 */
void ewmh_update_visible_name(xcb_window_t window, const char *name);

/**
 * Updates the _NET_CLIENT_LIST hint. Used for window listers.
 */
void ewmh_update_client_list(xcb_window_t *list, int num_windows);

/**
 * Updates the _NET_CLIENT_LIST_STACKING hint. Necessary to move tabs in
 * Chromium correctly.
 *
 * EWMH: These arrays contain all X Windows managed by the Window Manager.
 * _NET_CLIENT_LIST has initial mapping order, starting with the oldest window.
 * _NET_CLIENT_LIST_STACKING has bottom-to-top stacking order. These properties
 * SHOULD be set and updated by the Window Manager.
 *
 */
void ewmh_update_client_list_stacking(xcb_window_t *stack, int num_windows);

/**
 * Set or remove _NET_WM_STATE_STICKY on the window.
 *
 */
void ewmh_update_sticky(xcb_window_t window, bool sticky);

/**
 * Set or remove _NEW_WM_STATE_FOCUSED on the window.
 *
 */
void ewmh_update_focused(xcb_window_t window, bool is_focused);

/**
 * Set up the EWMH hints on the root window.
 *
 */
void ewmh_setup_hints(void);

/**
 * i3 currently does not support _NET_WORKAREA, because it does not correspond
 * to i3’s concept of workspaces. See also:
 * https://bugs.i3wm.org/539
 * https://bugs.i3wm.org/301
 * https://bugs.i3wm.org/1038
 *
 * We need to actively delete this property because some display managers (e.g.
 * LightDM) set it.
 *
 * EWMH: Contains a geometry for each desktop. These geometries specify an area
 * that is completely contained within the viewport. Work area SHOULD be used by
 * desktop applications to place desktop icons appropriately.
 *
 */
void ewmh_update_workarea(void);

/**
 * Returns the workspace container as enumerated by the EWMH desktop model.
 * Returns NULL if no workspace could be found for the index.
 *
 * This is the reverse of ewmh_get_workspace_index.
 *
 */
Con *ewmh_get_workspace_by_index(uint32_t idx);

/**
 * Returns the EWMH desktop index for the workspace the given container is on.
 * Returns NET_WM_DESKTOP_NONE if the desktop index cannot be determined.
 *
 * This is the reverse of ewmh_get_workspace_by_index.
 *
 */
uint32_t ewmh_get_workspace_index(Con *con);
