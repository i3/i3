/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * window.c: Updates window attributes (X11 hints/properties).
 *
 */
#pragma once

#include <config.h>

/**
 * Frees an i3Window and all its members.
 *
 */
void window_free(i3Window *win);

/**
 * Updates the WM_CLASS (consisting of the class and instance) for the
 * given window.
 *
 */
void window_update_class(i3Window *win, xcb_get_property_reply_t *prop);

/**
 * Updates the name by using _NET_WM_NAME (encoded in UTF-8) for the given
 * window. Further updates using window_update_name_legacy will be ignored.
 *
 */
void window_update_name(i3Window *win, xcb_get_property_reply_t *prop);

/**
 * Updates the name by using WM_NAME (encoded in COMPOUND_TEXT). We do not
 * touch what the client sends us but pass it to xcb_image_text_8. To get
 * proper unicode rendering, the application has to use _NET_WM_NAME (see
 * window_update_name()).
 *
 */
void window_update_name_legacy(i3Window *win, xcb_get_property_reply_t *prop);

/**
 * Updates the CLIENT_LEADER (logical parent window).
 *
 */
void window_update_leader(i3Window *win, xcb_get_property_reply_t *prop);

/**
 * Updates the TRANSIENT_FOR (logical parent window).
 *
 */
void window_update_transient_for(i3Window *win, xcb_get_property_reply_t *prop);

/**
 * Updates the _NET_WM_STRUT_PARTIAL (reserved pixels at the screen edges)
 *
 */
void window_update_strut_partial(i3Window *win, xcb_get_property_reply_t *prop);

/**
 * Updates the WM_WINDOW_ROLE
 *
 */
void window_update_role(i3Window *win, xcb_get_property_reply_t *prop);

/**
 * Updates the _NET_WM_WINDOW_TYPE property.
 *
 */
void window_update_type(i3Window *window, xcb_get_property_reply_t *reply);

/**
 * Updates the WM_NORMAL_HINTS
 *
 */
bool window_update_normal_hints(i3Window *win, xcb_get_property_reply_t *reply, xcb_get_geometry_reply_t *geom);

/**
 * Updates the WM_HINTS (we only care about the input focus handling part).
 *
 */
void window_update_hints(i3Window *win, xcb_get_property_reply_t *prop, bool *urgency_hint);

/**
 * Updates the MOTIF_WM_HINTS. The container's border style should be set to
 * `motif_border_style' if border style is not BS_NORMAL.
 *
 * i3 only uses this hint when it specifies a window should have no
 * title bar, or no decorations at all, which is how most window managers
 * handle it.
 *
 * The EWMH spec intended to replace Motif hints with _NET_WM_WINDOW_TYPE, but
 * it is still in use by popular widget toolkits such as GTK+ and Java AWT.
 *
 */
bool window_update_motif_hints(i3Window *win, xcb_get_property_reply_t *prop, border_style_t *motif_border_style);

/**
 * Updates the WM_CLIENT_MACHINE
 *
 */
void window_update_machine(i3Window *win, xcb_get_property_reply_t *prop);

/**
 * Updates the _NET_WM_ICON
 *
 */
void window_update_icon(i3Window *win, xcb_get_property_reply_t *prop);
