/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * floating.c: Floating windows.
 *
 */
#pragma once

#include <config.h>

#include "tree.h"

/** Callback for dragging */
typedef void (*callback_t)(Con *, Rect *, uint32_t, uint32_t, const void *);

/** Macro to create a callback function for dragging */
#define DRAGGING_CB(name)                                      \
    static void name(Con *con, Rect *old_rect, uint32_t new_x, \
                     uint32_t new_y, const void *extra)

/** On which border was the dragging initiated? */
typedef enum { BORDER_LEFT = (1 << 0),
               BORDER_RIGHT = (1 << 1),
               BORDER_TOP = (1 << 2),
               BORDER_BOTTOM = (1 << 3) } border_t;

/**
 * Enables floating mode for the given container by detaching it from its
 * parent, creating a new container around it and storing this container in the
 * floating_windows list of the workspace.
 *
 */
void floating_enable(Con *con, bool automatic);

/**
 * Disables floating mode for the given container by re-attaching the container
 * to its old parent.
 *
 */
void floating_disable(Con *con, bool automatic);

/**
 * Calls floating_enable() for tiling containers and floating_disable() for
 * floating containers.
 *
 * If the automatic flag is set to true, this was an automatic update by a
 * change of the window class from the application which can be overwritten by
 * the user.
 *
 */
void toggle_floating_mode(Con *con, bool automatic);

/**
 * Raises the given container in the list of floating containers
 *
 */
void floating_raise_con(Con *con);

/**
 * Checks if con’s coordinates are within its workspace and re-assigns it to
 * the actual workspace if not.
 *
 */
bool floating_maybe_reassign_ws(Con *con);

/**
 * Centers a floating con above the specified rect.
 *
 */
void floating_center(Con *con, Rect rect);

/**
 * Moves the given floating con to the current pointer position.
 *
 */
void floating_move_to_pointer(Con *con);

/**
 * Called when the user clicked on the titlebar of a floating window.
 * Calls the drag_pointer function with the drag_window callback
 *
 */
void floating_drag_window(Con *con, const xcb_button_press_event_t *event);

/**
 * Called when the user clicked on a floating window while holding the
 * floating_modifier and the right mouse button.
 * Calls the drag_pointer function with the resize_window callback
 *
 */
void floating_resize_window(Con *con, const bool proportional, const xcb_button_press_event_t *event);

/**
 * Called when a floating window is created or resized.  This function resizes
 * the window if its size is higher or lower than the configured maximum/minimum
 * size, respectively or when adjustments are needed to conform to the
 * configured size increments or aspect ratio limits.
 *
 * When prefer_height is true and the window needs to be resized because of the
 * configured aspect ratio, the width is adjusted first, preserving the previous
 * height.
 *
 */
void floating_check_size(Con *floating_con, bool prefer_height);

/**
 * This is the return value of a drag operation like drag_pointer.
 *
 * DRAGGING will indicate the drag action is still in progress and can be
 * continued or resolved.
 *
 * DRAG_SUCCESS will indicate the intention of the drag action should be
 * carried out.
 *
 * DRAG_REVERT will indicate an attempt should be made to restore the state of
 * the involved windows to their condition before the drag.
 *
 * DRAG_ABORT will indicate that the intention of the drag action cannot be
 * carried out (e.g. because the window has been unmapped).
 *
 */
typedef enum {
    DRAGGING = 0,
    DRAG_SUCCESS,
    DRAG_REVERT,
    DRAG_ABORT
} drag_result_t;

/**
 * This function grabs your pointer and keyboard and lets you drag stuff around
 * (borders). Every time you move your mouse, an XCB_MOTION_NOTIFY event will
 * be received and the given callback will be called with the parameters
 * specified (client, border on which the click originally was), the original
 * rect of the client, the event and the new coordinates (x, y).
 *
 */
drag_result_t drag_pointer(Con *con, const xcb_button_press_event_t *event,
                           xcb_window_t confine_to, border_t border, int cursor,
                           callback_t callback, const void *extra);

/**
 * Repositions the CT_FLOATING_CON to have the coordinates specified by
 * newrect, but only if the coordinates are not out-of-bounds. Also reassigns
 * the floating con to a different workspace if this move was across different
 * outputs.
 *
 */
bool floating_reposition(Con *con, Rect newrect);

/**
 * Sets size of the CT_FLOATING_CON to specified dimensions. Might limit the
 * actual size with regard to size constraints taken from user settings.
 * Additionally, the dimensions may be upscaled until they're divisible by the
 * window's size hints.
 *
 */
void floating_resize(Con *floating_con, uint32_t x, uint32_t y);

/**
 * Fixes the coordinates of the floating window whenever the window gets
 * reassigned to a different output (or when the output’s rect changes).
 *
 */
void floating_fix_coordinates(Con *con, Rect *old_rect, Rect *new_rect);
