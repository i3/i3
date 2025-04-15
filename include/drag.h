/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * drag.c: click and drag.
 *
 */
#pragma once

#include <config.h>

/** Callback for dragging */
typedef void (*callback_t)(Con *, Rect *, uint32_t, uint32_t,
                           const xcb_button_press_event_t *, const void *);

/** Macro to create a callback function for dragging */
#define DRAGGING_CB(name)                                                      \
    static void name(Con *con, Rect *old_rect, uint32_t new_x, uint32_t new_y, \
                     const xcb_button_press_event_t *event, const void *extra)

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
 * specified (client, the original event), the original rect of the client,
 * and the new coordinates (x, y).
 *
 * If use_threshold is set, dragging only starts after the user moves the
 * pointer past a certain threshold. That is, the cursor will not be set and the
 * callback will not be called until then.
 *
 */
drag_result_t drag_pointer(Con *con, const xcb_button_press_event_t *event,
                           xcb_window_t confine_to, int cursor,
                           bool use_threshold, callback_t callback,
                           const void *extra);
