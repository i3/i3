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
#ifndef _FLOATING_H
#define _FLOATING_H

/** Callback for dragging */
typedef void(*callback_t)(xcb_connection_t*, Client*, Rect*, uint32_t, uint32_t, void*);

/** Macro to create a callback function for dragging */
#define DRAGGING_CB(name) \
        static void name(xcb_connection_t *conn, Client *client, \
                         Rect *old_rect, uint32_t new_x, uint32_t new_y, \
                         void *extra)

/** On which border was the dragging initiated? */
typedef enum { BORDER_LEFT   = (1 << 0),
               BORDER_RIGHT  = (1 << 1),
               BORDER_TOP    = (1 << 2),
               BORDER_BOTTOM = (1 << 3)} border_t;

/**
 * Enters floating mode for the given client.  Correctly takes care of the
 * position/size (separately stored for tiling/floating mode) and
 * repositions/resizes/redecorates the client.
 *
 * If the automatic flag is set to true, this was an automatic update by a
 * change of the window class from the application which can be overwritten by
 * the user.
 *
 */
void toggle_floating_mode(xcb_connection_t *conn, Client *client,
                          bool automatic);

/**
 * Removes the floating client from its workspace and attaches it to the new
 * workspace. This is centralized here because it may happen if you move it
 * via keyboard and if you move it using your mouse.
 *
 */
void floating_assign_to_workspace(Client *client, Workspace *new_workspace);

/**
 * Called whenever the user clicks on a border (not the titlebar!) of a
 * floating window. Determines on which border the user clicked and launches
 * the drag_pointer function with the resize_callback.
 *
 */
int floating_border_click(xcb_connection_t *conn, Client *client,
                          xcb_button_press_event_t *event);

/**
 * Called when the user clicked on the titlebar of a floating window.
 * Calls the drag_pointer function with the drag_window callback
 *
 */
void floating_drag_window(xcb_connection_t *conn, Client *client,
                          xcb_button_press_event_t *event);

/**
 * Called when the user clicked on a floating window while holding the
 * floating_modifier and the right mouse button.
 * Calls the drag_pointer function with the resize_window callback
 *
 */
void floating_resize_window(xcb_connection_t *conn, Client *client,
                            bool proportional, xcb_button_press_event_t *event);

/**
 * Changes focus in the given direction for floating clients.
 *
 * Changing to the left/right means going to the previous/next floating client,
 * changing to top/bottom means cycling through the Z-index.
 *
 */
void floating_focus_direction(xcb_connection_t *conn, Client *currently_focused,
                              direction_t direction);

/**
 * Moves the client 10px to the specified direction.
 *
 */
void floating_move(xcb_connection_t *conn, Client *currently_focused,
                   direction_t direction);

/**
 * Hides all floating clients (or show them if they are currently hidden) on
 * the specified workspace.
 *
 */
void floating_toggle_hide(xcb_connection_t *conn, Workspace *workspace);

/**
 * This function grabs your pointer and lets you drag stuff around (borders).
 * Every time you move your mouse, an XCB_MOTION_NOTIFY event will be received
 * and the given callback will be called with the parameters specified (client,
 * border on which the click originally was), the original rect of the client,
 * the event and the new coordinates (x, y).
 *
 */
void drag_pointer(xcb_connection_t *conn, Client *client, xcb_button_press_event_t *event,
                  xcb_window_t confine_to, border_t border, callback_t callback,
                  void *extra);

#endif
