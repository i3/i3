/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2011 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#ifndef _XCB_H
#define _XCB_H

#include "data.h"
#include "xcursor.h"

#define _NET_WM_STATE_REMOVE    0
#define _NET_WM_STATE_ADD       1
#define _NET_WM_STATE_TOGGLE    2

/** This is the equivalent of XC_left_ptr. I’m not sure why xcb doesn’t have a
 * constant for that. */
#define XCB_CURSOR_LEFT_PTR     68
#define XCB_CURSOR_SB_H_DOUBLE_ARROW 108
#define XCB_CURSOR_SB_V_DOUBLE_ARROW 116

/* from X11/keysymdef.h */
#define XCB_NUM_LOCK                    0xff7f

/* The event masks are defined here because we don’t only set them once but we
   need to set slight variations of them (without XCB_EVENT_MASK_ENTER_WINDOW
   while rendering the layout) */
/** The XCB_CW_EVENT_MASK for the child (= real window) */
#define CHILD_EVENT_MASK (XCB_EVENT_MASK_PROPERTY_CHANGE | \
                          XCB_EVENT_MASK_STRUCTURE_NOTIFY | \
                          XCB_EVENT_MASK_FOCUS_CHANGE)

/** The XCB_CW_EVENT_MASK for its frame */
#define FRAME_EVENT_MASK (XCB_EVENT_MASK_BUTTON_PRESS |          /* …mouse is pressed/released */ \
                          XCB_EVENT_MASK_BUTTON_RELEASE | \
                          XCB_EVENT_MASK_POINTER_MOTION |        /* …mouse is moved */ \
                          XCB_EVENT_MASK_EXPOSURE |              /* …our window needs to be redrawn */ \
                          XCB_EVENT_MASK_STRUCTURE_NOTIFY |      /* …the frame gets destroyed */ \
                          XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | /* …the application tries to resize itself */ \
                          XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |   /* …subwindows get notifies */ \
                          XCB_EVENT_MASK_ENTER_WINDOW)           /* …user moves cursor inside our window */

#define xmacro(atom) xcb_atom_t A_ ## atom;
#include "atoms.xmacro"
#undef xmacro

extern unsigned int xcb_numlock_mask;

/**
 * Loads a font for usage, also getting its height. If fallback is true,
 * i3 loads 'fixed' or '-misc-*' if the font cannot be found instead of
 * exiting.
 *
 */
i3Font load_font(const char *pattern, bool fallback);

/**
 * Returns the colorpixel to use for the given hex color (think of HTML).
 *
 * The hex_color has to start with #, for example #FF00FF.
 *
 * NOTE that get_colorpixel() does _NOT_ check the given color code for
 * validity.  This has to be done by the caller.
 *
 */
uint32_t get_colorpixel(char *hex);

/**
 * Convenience wrapper around xcb_create_window which takes care of depth,
 * generating an ID and checking for errors.
 *
 */
xcb_window_t create_window(xcb_connection_t *conn, Rect r, uint16_t window_class,
        enum xcursor_cursor_t cursor, bool map, uint32_t mask, uint32_t *values);

/**
 * Changes a single value in the graphic context (so one doesn’t have to
 * define an array of values)
 *
 */
void xcb_change_gc_single(xcb_connection_t *conn, xcb_gcontext_t gc,
                          uint32_t mask, uint32_t value);

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
 * Generates a configure_notify event and sends it to the given window
 * Applications need this to think they’ve configured themselves correctly.
 * The truth is, however, that we will manage them.
 *
 */
void fake_configure_notify(xcb_connection_t *conn, Rect r, xcb_window_t window, int border_width);

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
void send_take_focus(xcb_window_t window);

/**
 * Finds out which modifier mask is the one for numlock, as the user may
 * change this.
 *
 */
void xcb_get_numlock_mask(xcb_connection_t *conn);

/**
 * Raises the given window (typically client->frame) above all other windows
 *
 */
void xcb_raise_window(xcb_connection_t *conn, xcb_window_t window);

/**
 * Calculate the width of the given text (16-bit characters, UCS) with given
 * real length (amount of glyphs) using the given font.
 *
 */
int predict_text_width(char *text, int length);

/**
 * Configures the given window to have the size/position specified by given rect
 *
 */
void xcb_set_window_rect(xcb_connection_t *conn, xcb_window_t window, Rect r);


bool xcb_reply_contains_atom(xcb_get_property_reply_t *prop, xcb_atom_t atom);

/**
 * Moves the mouse pointer into the middle of rect.
 *
 */
void xcb_warp_pointer_rect(xcb_connection_t *conn, Rect *rect);

#endif
