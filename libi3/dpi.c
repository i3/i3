/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "libi3.h"

#include <math.h>
#include <stdlib.h>
#include <xcb/xcb_xrm.h>

static long dpi;

static long init_dpi_fallback(void) {
    return (double)root_screen->height_in_pixels * 25.4 / (double)root_screen->height_in_millimeters;
}

/*
 * Initialize the DPI setting.
 * This will use the 'Xft.dpi' X resource if available and fall back to
 * guessing the correct value otherwise.
 */
void init_dpi(void) {
    xcb_xrm_database_t *database = NULL;
    char *resource = NULL;

    if (conn == NULL) {
        goto init_dpi_end;
    }

    database = xcb_xrm_database_from_default(conn);
    if (database == NULL) {
        ELOG("Failed to open the resource database.\n");
        goto init_dpi_end;
    }

    xcb_xrm_resource_get_string(database, "Xft.dpi", NULL, &resource);
    if (resource == NULL) {
        DLOG("Resource Xft.dpi not specified, skipping.\n");
        goto init_dpi_end;
    }

    char *endptr;
    double in_dpi = strtod(resource, &endptr);
    if (in_dpi == HUGE_VAL || dpi < 0 || *endptr != '\0' || endptr == resource) {
        ELOG("Xft.dpi = %s is an invalid number and couldn't be parsed.\n", resource);
        dpi = 0;
        goto init_dpi_end;
    }
    dpi = (long)round(in_dpi);

    DLOG("Found Xft.dpi = %ld.\n", dpi);

init_dpi_end:
    if (resource != NULL) {
        free(resource);
    }

    if (database != NULL) {
        xcb_xrm_database_free(database);
    }

    if (dpi == 0) {
        DLOG("Using fallback for calculating DPI.\n");
        dpi = init_dpi_fallback();
        DLOG("Using dpi = %ld\n", dpi);
    }
}

/*
 * This function returns the value of the DPI setting.
 *
 */
long get_dpi_value(void) {
    return dpi;
}

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
