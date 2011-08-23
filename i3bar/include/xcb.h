/*
 * i3bar - an xcb-based status- and ws-bar for i3
 *
 * © 2010-2011 Axel Wagner and contributors
 *
 * See file LICNSE for license information
 *
 */
#ifndef XCB_H_
#define XCB_H_

#include <stdint.h>
//#include "outputs.h"

#ifdef XCB_COMPAT
#define XCB_ATOM_CARDINAL CARDINAL
#endif

#define _NET_SYSTEM_TRAY_ORIENTATION_HORZ 0
#define _NET_SYSTEM_TRAY_ORIENTATION_VERT 1
#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2
#define XEMBED_MAPPED                   (1 << 0)
#define XEMBED_EMBEDDED_NOTIFY		0

struct xcb_color_strings_t {
    char *bar_fg;
    char *bar_bg;
    char *active_ws_fg;
    char *active_ws_bg;
    char *inactive_ws_fg;
    char *inactive_ws_bg;
    char *focus_ws_bg;
    char *focus_ws_fg;
    char *urgent_ws_bg;
    char *urgent_ws_fg;
};

typedef struct xcb_colors_t xcb_colors_t;

/*
 * Initialize xcb and use the specified fontname for text-rendering
 *
 */
char *init_xcb(char *fontname);

/*
 * Initialize the colors
 *
 */
void init_colors(const struct xcb_color_strings_t *colors);

/*
 * Cleanup the xcb-stuff.
 * Called once, before the program terminates.
 *
 */
void clean_xcb();

/*
 * Get the earlier requested atoms and save them in the prepared data-structure
 *
 */
void get_atoms();

/*
 * Destroy the bar of the specified output
 *
 */
void destroy_window(i3_output *output);

/*
 * Reallocate the statusline-buffer
 *
 */
void realloc_sl_buffer();

/*
 * Reconfigure all bars and create new for newly activated outputs
 *
 */
void reconfig_windows();

/*
 * Render the bars, with buttons and statusline
 *
 */
void draw_bars();

/*
 * Redraw the bars, i.e. simply copy the buffer to the barwindow
 *
 */
void redraw_bars();

/*
 * Predicts the length of text based on cached data.
 * The string has to be encoded in ucs2 and glyph_len has to be the length
 * of the string (in glyphs).
 *
 */
uint32_t predict_text_extents(xcb_char2b_t *text, uint32_t length);

#endif
