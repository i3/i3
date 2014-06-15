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
    const int dpi = (double)root_screen->height_in_pixels * 25.4 /
                    (double)root_screen->height_in_millimeters;
    return ceil((dpi / 96.0) * logical);
}
