/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2012 Axel Wagner and contributors (see also: LICENSE)
 *
 * xcb.c: Communicating with X
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
    char *active_ws_border;
    char *inactive_ws_fg;
    char *inactive_ws_bg;
    char *inactive_ws_border;
    char *focus_ws_bg;
    char *focus_ws_fg;
    char *focus_ws_border;
    char *urgent_ws_bg;
    char *urgent_ws_fg;
    char *urgent_ws_border;
};

typedef struct xcb_colors_t xcb_colors_t;

/*
 * Early initialization of the connection to X11: Everything which does not
 * depend on 'config'.
 *
 */
char *init_xcb_early();

/**
 * Initialization which depends on 'config' being usable. Called after the
 * configuration has arrived.
 *
 */
void init_xcb_late(char *fontname);

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
void clean_xcb(void);

/*
 * Get the earlier requested atoms and save them in the prepared data-structure
 *
 */
void get_atoms(void);

/*
 * Reparents all tray clients of the specified output to the root window. This
 * is either used when shutting down, when an output appears (xrandr --output
 * VGA1 --off) or when the primary output changes.
 *
 * Applications using the tray will start the protocol from the beginning again
 * afterwards.
 *
 */
void kick_tray_clients(i3_output *output);

/*
 * Destroy the bar of the specified output
 *
 */
void destroy_window(i3_output *output);

/*
 * Reallocate the statusline-buffer
 *
 */
void realloc_sl_buffer(void);

/*
 * Reconfigure all bars and create new for newly activated outputs
 *
 */
void reconfig_windows(void);

/*
 * Render the bars, with buttons and statusline
 *
 */
void draw_bars(bool force_unhide);

/*
 * Redraw the bars, i.e. simply copy the buffer to the barwindow
 *
 */
void redraw_bars(void);

/*
 * Set the current binding mode
 *
 */
void set_current_mode(struct mode *mode);

#endif
