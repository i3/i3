/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * ewmh.c: Functions to get/set certain EWMH properties easily.
 *
 */
#include <stdint.h>

#include "data.h"
#include "table.h"
#include "i3.h"
#include "xcb.h"

/*
 * Updates _NET_CURRENT_DESKTOP with the current desktop number.
 *
 * EWMH: The index of the current desktop. This is always an integer between 0
 * and _NET_NUMBER_OF_DESKTOPS - 1.
 *
 */
void ewmh_update_current_desktop() {
        uint32_t current_desktop = c_ws->num;
        xcb_change_property(global_conn, XCB_PROP_MODE_REPLACE, root,
                            atoms[_NET_CURRENT_DESKTOP], CARDINAL, 32, 1,
                            &current_desktop);
}
