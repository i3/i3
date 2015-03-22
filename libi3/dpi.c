/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2014 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "libi3.h"
#include <math.h>

extern xcb_screen_t *root_screen;

/*
 * Convert a logical amount of pixels (e.g. 2 pixels on a “standard” 96 DPI
 * screen) to a corresponding amount of physical pixels on a standard or retina
 * screen, e.g. 5 pixels on a 227 DPI MacBook Pro 13" Retina screen.
 *
 */
int logical_px(const int logical) {
    if (root_screen == NULL) {
        /* Dpi info may not be available when parsing a config without an X
         * server, such as for config file validation. */
        return logical;
    }

    const int dpi = (double)root_screen->height_in_pixels * 25.4 /
                    (double)root_screen->height_in_millimeters;
    /* There are many misconfigurations out there, i.e. systems with screens
     * whose dpi is in fact higher than 96 dpi, but not significantly higher,
     * so software was never adapted. We could tell people to reconfigure their
     * systems to 96 dpi in order to get the behavior they expect/are used to,
     * but since we can easily detect this case in code, let’s do it for them.
     */
    if ((dpi / 96.0) < 1.25)
        return logical;
    return ceil((dpi / 96.0) * logical);
}
