/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * ewmh.c: Get/set certain EWMH properties easily.
 *
 */
#pragma once

/**
 * Updates _NET_CURRENT_DESKTOP with the current desktop number.
 *
 * EWMH: The index of the current desktop. This is always an integer between 0
 * and _NET_NUMBER_OF_DESKTOPS - 1.
 *
 */
void ewmh_update_current_desktop(void);

/**
 * Updates _NET_NUMBER_OF_DESKTOPS which we interpret as the number of
 * noninternal workspaces.
 */
void ewmh_update_number_of_desktops(void);

/**
 * Updates _NET_DESKTOP_NAMES: "The names of all virtual desktops. This is a
 * list of NULL-terminated strings in UTF-8 encoding"
 */
void ewmh_update_desktop_names(void);

/**
 * Updates _NET_DESKTOP_VIEWPORT, which is an array of pairs of cardinals that
 * define the top left corner of each desktop's viewport.
 */
void ewmh_update_desktop_viewport(void);

/**
 * Updates _NET_ACTIVE_WINDOW with the currently focused window.
 *
 * EWMH: The window ID of the currently active window or None if no window has
 * the focus.
 *
 */
void ewmh_update_active_window(xcb_window_t window);

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
 * Set up the EWMH hints on the root window.
 *
 */
void ewmh_setup_hints(void);

/**
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
void ewmh_update_workarea(void);
