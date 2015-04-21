/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * restore_layout.c: Everything for restored containers that is not pure state
 *                   parsing (which can be found in load_layout.c).
 *
 */
#pragma once

/**
 * Opens a separate connection to X11 for placeholder windows when restoring
 * layouts. This is done as a safety measure (users can xkill a placeholder
 * window without killing their window manager) and for better isolation, both
 * on the wire to X11 and thus also in the code.
 *
 */
void restore_connect(void);

/**
 * Open placeholder windows for all children of parent. The placeholder window
 * will vanish as soon as a real window is swallowed by the container. Until
 * then, it exposes the criteria that must be fulfilled for a window to be
 * swallowed by this container.
 *
 */
void restore_open_placeholder_windows(Con *con);

/**
 * Kill the placeholder window, if placeholder refers to a placeholder window.
 * This function is called when manage.c puts a window into an existing
 * container. In order not to leak resources, we need to destroy the window and
 * all associated X11 objects (pixmap/gc).
 *
 */
bool restore_kill_placeholder(xcb_window_t placeholder);
