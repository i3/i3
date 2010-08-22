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

int font_height;

/*
 * Initialize xcb and use the specified fontname for text-rendering
 *
 */
void init_xcb();

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
 * Calculate the rendered width of a string with the configured font.
 * The string has to be encoded in ucs2 and glyph_len has to be the length
 * of the string (in width)
 *
 */
int get_string_width(xcb_char2b_t *string, int glyph_len);

#endif
