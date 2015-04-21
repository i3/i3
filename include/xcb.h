/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * xcb.c: Helper functions for easier usage of XCB
 *
 */
#pragma once

#include "data.h"
#include "xcursor.h"

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2

/** This is the equivalent of XC_left_ptr. I’m not sure why xcb doesn’t have a
 * constant for that. */
#define XCB_CURSOR_LEFT_PTR 68
#define XCB_CURSOR_SB_H_DOUBLE_ARROW 108
#define XCB_CURSOR_SB_V_DOUBLE_ARROW 116
#define XCB_CURSOR_WATCH 150

/* from X11/keysymdef.h */
#define XCB_NUM_LOCK 0xff7f

/* The event masks are defined here because we don’t only set them once but we
   need to set slight variations of them (without XCB_EVENT_MASK_ENTER_WINDOW
   while rendering the layout) */
/** The XCB_CW_EVENT_MASK for the child (= real window) */
#define CHILD_EVENT_MASK (XCB_EVENT_MASK_PROPERTY_CHANGE |  \
                          XCB_EVENT_MASK_STRUCTURE_NOTIFY | \
                          XCB_EVENT_MASK_FOCUS_CHANGE)

/** The XCB_CW_EVENT_MASK for its frame */
#define FRAME_EVENT_MASK (XCB_EVENT_MASK_BUTTON_PRESS | /* …mouse is pressed/released */                       \
                          XCB_EVENT_MASK_BUTTON_RELEASE |                                                        \
                          XCB_EVENT_MASK_POINTER_MOTION |        /* …mouse is moved */                         \
                          XCB_EVENT_MASK_EXPOSURE |              /* …our window needs to be redrawn */         \
                          XCB_EVENT_MASK_STRUCTURE_NOTIFY |      /* …the frame gets destroyed */               \
                          XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | /* …the application tries to resize itself */ \
                          XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |   /* …subwindows get notifies */                \
                          XCB_EVENT_MASK_ENTER_WINDOW)           /* …user moves cursor inside our window */

#define ROOT_EVENT_MASK (XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |                                       \
                         XCB_EVENT_MASK_BUTTON_PRESS |                                                \
                         XCB_EVENT_MASK_STRUCTURE_NOTIFY | /* when the user adds a screen (e.g. video \
                                                                  projector), the root window gets a  \
                                                                  ConfigureNotify */                  \
                         XCB_EVENT_MASK_POINTER_MOTION |                                              \
                         XCB_EVENT_MASK_PROPERTY_CHANGE |                                             \
                         XCB_EVENT_MASK_ENTER_WINDOW)

#define xmacro(atom) xcb_atom_t A_##atom;
#include "atoms.xmacro"
#undef xmacro

extern unsigned int xcb_numlock_mask;

/**
 * Convenience wrapper around xcb_create_window which takes care of depth,
 * generating an ID and checking for errors.
 *
 */
xcb_window_t create_window(xcb_connection_t *conn, Rect r, uint16_t depth, xcb_visualid_t visual,
                           uint16_t window_class, enum xcursor_cursor_t cursor, bool map, uint32_t mask, uint32_t *values);

/**
 * Draws a line from x,y to to_x,to_y using the given color
 *
 */
void xcb_draw_line(xcb_connection_t *conn, xcb_drawable_t drawable,
                   xcb_gcontext_t gc, uint32_t colorpixel, uint32_t x,
                   uint32_t y, uint32_t to_x, uint32_t to_y);

/**
 * Draws a rectangle from x,y with width,height using the given color
 *
 */
void xcb_draw_rect(xcb_connection_t *conn, xcb_drawable_t drawable,
                   xcb_gcontext_t gc, uint32_t colorpixel, uint32_t x,
                   uint32_t y, uint32_t width, uint32_t height);

/**
 * Generates a configure_notify_event with absolute coordinates (relative to
 * the X root window, not to the client’s frame) for the given client.
 *
 */
void fake_absolute_configure_notify(Con *con);

/**
 * Sends the WM_TAKE_FOCUS ClientMessage to the given window
 *
 */
void send_take_focus(xcb_window_t window, xcb_timestamp_t timestamp);

/**
 * Raises the given window (typically client->frame) above all other windows
 *
 */
void xcb_raise_window(xcb_connection_t *conn, xcb_window_t window);

/**
 * Configures the given window to have the size/position specified by given rect
 *
 */
void xcb_set_window_rect(xcb_connection_t *conn, xcb_window_t window, Rect r);

/**
 * Returns the first supported _NET_WM_WINDOW_TYPE atom.
 *
 */
xcb_atom_t xcb_get_preferred_window_type(xcb_get_property_reply_t *reply);

/**
 * Returns true if the given reply contains the given data.
 *
 */
bool xcb_reply_contains_atom(xcb_get_property_reply_t *prop, xcb_atom_t atom);

/**
 * Moves the mouse pointer into the middle of rect.
 *
 */
void xcb_warp_pointer_rect(xcb_connection_t *conn, Rect *rect);

/**
 * Set the cursor of the root window to the given cursor id.
 * This function should only be used if xcursor_supported == false.
 * Otherwise, use xcursor_set_root_cursor().
 *
 */
void xcb_set_root_cursor(int cursor);

/**
 * Get depth of visual specified by visualid
 *
 */
uint16_t get_visual_depth(xcb_visualid_t visual_id);

/**
 * Get visualid with specified depth
 *
 */
xcb_visualid_t get_visualid_by_depth(uint16_t depth);
