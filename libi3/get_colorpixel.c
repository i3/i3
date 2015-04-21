/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include <stdlib.h>
#include <stdint.h>

#include "libi3.h"

/*
 * Returns the colorpixel to use for the given hex color (think of HTML). Only
 * works for true-color (vast majority of cases) at the moment, avoiding a
 * roundtrip to X11.
 *
 * The hex_color has to start with #, for example #FF00FF.
 *
 * NOTE that get_colorpixel() does _NOT_ check the given color code for validity.
 * This has to be done by the caller.
 *
 * NOTE that this function may in the future rely on a global xcb_connection_t
 * variable called 'conn' to be present.
 *
 */
uint32_t get_colorpixel(const char *hex) {
    char strgroups[3][3] = {{hex[1], hex[2], '\0'},
                            {hex[3], hex[4], '\0'},
                            {hex[5], hex[6], '\0'}};
    uint8_t r = strtol(strgroups[0], NULL, 16);
    uint8_t g = strtol(strgroups[1], NULL, 16);
    uint8_t b = strtol(strgroups[2], NULL, 16);

    /* We set the first 8 bits high to have 100% opacity in case of a 32 bit
     * color depth visual. */
    return (0xFF << 24) | (r << 16 | g << 8 | b);
}
