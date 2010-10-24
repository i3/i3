/*
 * i3bar - an xcb-based status- and ws-bar for i3
 *
 * © 2010 Axel Wagner and contributors
 *
 * See file LICNSE for license information
 *
 */
#ifndef XCB_H_
#define XCB_H_

#include <stdint.h>

struct colors_t {
    char *bar_fg;
    char *bar_bg;
    char *active_ws_fg;
    char *active_ws_bg;
    char *inactive_ws_fg;
    char *inactive_ws_bg;
    char *urgent_ws_bg;
    char *urgent_ws_fg;
};

struct parsed_colors_t {
    uint32_t bar_fg;
    uint32_t bar_bg;
    uint32_t active_ws_fg;
    uint32_t active_ws_bg;
    uint32_t inactive_ws_fg;
    uint32_t inactive_ws_bg;
    uint32_t urgent_ws_bg;
    uint32_t urgent_ws_fg;
};

/*
 * Initialize xcb and use the specified fontname for text-rendering
 *
 */
void init_xcb();

/*
 * Initialize the colors
 *
 */
void init_colors(const struct colors_t *colors);

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
