/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#ifndef _HANDLERS_H
#define _HANDLERS_H

#include <xcb/randr.h>

/**
 * There was a key press. We compare this key code with our bindings table and
 * pass the bound action to parse_command().
 *
 */
int handle_key_press(void *ignored, xcb_connection_t *conn,
                     xcb_key_press_event_t *event);

/**
 * When the user moves the mouse pointer onto a window, this callback gets
 * called.
 *
 */
int handle_enter_notify(void *ignored, xcb_connection_t *conn,
                        xcb_enter_notify_event_t *event);

/**
 * When the user moves the mouse but does not change the active window
 * (e.g. when having no windows opened but moving mouse on the root screen
 * and crossing virtual screen boundaries), this callback gets called.
 *
 */
int handle_motion_notify(void *ignored, xcb_connection_t *conn,
                         xcb_motion_notify_event_t *event);

/**
 * Called when the keyboard mapping changes (for example by using Xmodmap),
 * we need to update our key bindings then (re-translate symbols).
 *
 */
int handle_mapping_notify(void *ignored, xcb_connection_t *conn,
                          xcb_mapping_notify_event_t *event);

/**
 * Checks if the button press was on a stack window, handles focus setting and
 * returns true if so, or false otherwise.
 *
 */
int handle_button_press(void *ignored, xcb_connection_t *conn,
                        xcb_button_press_event_t *event);

/**
 * A new window appeared on the screen (=was mapped), so let’s manage it.
 *
 */
int handle_map_request(void *prophs, xcb_connection_t *conn,
                       xcb_map_request_event_t *event);

/**
 * Configuration notifies are only handled because we need to set up ignore
 * for the following enter notify events
 *
 */
int handle_configure_event(void *prophs, xcb_connection_t *conn, xcb_configure_notify_event_t *event);

/**
 * Gets triggered upon a RandR screen change event, that is when the user
 * changes the screen configuration in any way (mode, position, …)
 *
 */
int handle_screen_change(void *prophs, xcb_connection_t *conn,
                         xcb_generic_event_t *e);

/**
 * Configure requests are received when the application wants to resize
 * windows on their own.
 *
 * We generate a synthethic configure notify event to signalize the client its
 * "new" position.
 *
 */
int handle_configure_request(void *prophs, xcb_connection_t *conn,
                             xcb_configure_request_event_t *event);

/**
 * Our window decorations were unmapped. That means, the window will be killed
 * now, so we better clean up before.
 *
 */
int handle_unmap_notify_event(void *data, xcb_connection_t *conn, xcb_unmap_notify_event_t *event);

/**
 * Called when a window changes its title
 *
 */
int handle_windowname_change(void *data, xcb_connection_t *conn, uint8_t state,
                             xcb_window_t window, xcb_atom_t atom,
                             xcb_get_property_reply_t *prop);

/**
 * We handle legacy window names (titles) which are in COMPOUND_TEXT
 * encoding. However, we just pass them along, so when containing non-ASCII
 * characters, those will be rendering incorrectly. In order to correctly
 * render unicode window titles in i3, an application has to set _NET_WM_NAME,
 * which is in UTF-8 encoding.
 *
 * On every update, a message is put out to the user, so he may improve the
 * situation and update applications which display filenames in their title to
 * correctly use _NET_WM_NAME and therefore support unicode.
 *
 */
int handle_windowname_change_legacy(void *data, xcb_connection_t *conn,
                                    uint8_t state, xcb_window_t window,
                                    xcb_atom_t atom, xcb_get_property_reply_t
                                    *prop);

/**
 * Store the window classes for jumping to them later.
 *
 */
int handle_windowclass_change(void *data, xcb_connection_t *conn, uint8_t state,
                              xcb_window_t window, xcb_atom_t atom,
                              xcb_get_property_reply_t *prop);


/**
 * Expose event means we should redraw our windows (= title bar)
 *
 */
int handle_expose_event(void *data, xcb_connection_t *conn,
                        xcb_expose_event_t *event);

/**
 * Handle client messages (EWMH)
 *
 */
int handle_client_message(void *data, xcb_connection_t *conn,
                          xcb_client_message_event_t *event);

/**
 * Handles _NET_WM_WINDOW_TYPE changes
 *
 */
int handle_window_type(void *data, xcb_connection_t *conn, uint8_t state,
                       xcb_window_t window, xcb_atom_t atom,
                       xcb_get_property_reply_t *property);

/**
 * Handles the size hints set by a window, but currently only the part
 * necessary for displaying clients proportionally inside their frames
 * (mplayer for example)
 *
 * See ICCCM 4.1.2.3 for more details
 *
 */
int handle_normal_hints(void *data, xcb_connection_t *conn, uint8_t state,
                        xcb_window_t window, xcb_atom_t name,
                        xcb_get_property_reply_t *reply);

/**
 * Handles the WM_HINTS property for extracting the urgency state of the window.
 *
 */
int handle_hints(void *data, xcb_connection_t *conn, uint8_t state, xcb_window_t window,
                  xcb_atom_t name, xcb_get_property_reply_t *reply);

/**
 * Handles the transient for hints set by a window, signalizing that this
 * window is a popup window for some other window.
 *
 * See ICCCM 4.1.2.6 for more details
 *
 */
int handle_transient_for(void *data, xcb_connection_t *conn, uint8_t state,
                         xcb_window_t window, xcb_atom_t name,
                         xcb_get_property_reply_t *reply);

/**
 * Handles changes of the WM_CLIENT_LEADER atom which specifies if this is a
 * toolwindow (or similar) and to which window it belongs (logical parent).
 *
 */
int handle_clientleader_change(void *data, xcb_connection_t *conn,
                               uint8_t state, xcb_window_t window,
                               xcb_atom_t name, xcb_get_property_reply_t *prop);

#endif
