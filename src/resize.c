/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * This file contains the functions for resizing table columns/rows because
 * it’s actually lots of work, compared to the other handlers.
 *
 */
#include <stdlib.h>
#include <assert.h>

#include <xcb/xcb.h>
#include <xcb/xcb_event.h>

#include "i3.h"
#include "data.h"
#include "resize.h"
#include "util.h"
#include "xcb.h"
#include "debug.h"
#include "layout.h"
#include "randr.h"
#include "config.h"
#include "floating.h"
#include "workspace.h"
#include "log.h"

/*
 * This is an ugly data structure which we need because there is no standard
 * way of having nested functions (only available as a gcc extension at the
 * moment, clang doesn’t support it) or blocks (only available as a clang
 * extension and only on Mac OS X systems at the moment).
 *
 */
struct callback_params {
        resize_orientation_t orientation;
        Output *screen;
        xcb_window_t helpwin;
        uint32_t *new_position;
};

DRAGGING_CB(resize_callback) {
        struct callback_params *params = extra;
        Output *screen = params->screen;
        DLOG("new x = %d, y = %d\n", new_x, new_y);
        if (params->orientation == O_VERTICAL) {
                /* Check if the new coordinates are within screen boundaries */
                if (new_x > (screen->rect.x + screen->rect.width - 25) ||
                    new_x < (screen->rect.x + 25))
                        return;

                *(params->new_position) = new_x;
                xcb_configure_window(conn, params->helpwin, XCB_CONFIG_WINDOW_X, params->new_position);
        } else {
                if (new_y > (screen->rect.y + screen->rect.height - 25) ||
                    new_y < (screen->rect.y + 25))
                        return;

                *(params->new_position) = new_y;
                xcb_configure_window(conn, params->helpwin, XCB_CONFIG_WINDOW_Y, params->new_position);
        }

        xcb_flush(conn);
}

/*
 * Renders the resize window between the first/second container and resizes
 * the table column/row.
 *
 */
int resize_graphical_handler(xcb_connection_t *conn, Workspace *ws, int first, int second,
                             resize_orientation_t orientation, xcb_button_press_event_t *event) {
        uint32_t new_position;
        Output *screen = get_output_containing(event->root_x, event->root_y);
        if (screen == NULL) {
                ELOG("BUG: No screen found at this position (%d, %d)\n", event->root_x, event->root_y);
                return 1;
        }

        /* We cannot use the X root window's width_in_pixels or height_in_pixels
         * attributes here since they are not updated when you configure new
         * screens during runtime. Instead, we just use the most right and most
         * bottom Xinerama screen and use their position + width/height to get
         * the area of pixels currently in use */
        Output *most_right = get_output_most(D_RIGHT, screen),
               *most_bottom = get_output_most(D_DOWN, screen);

        DLOG("event->event_x = %d, event->root_x = %d\n", event->event_x, event->root_x);

        DLOG("Screen dimensions: (%d, %d) %d x %d\n", screen->rect.x, screen->rect.y, screen->rect.width, screen->rect.height);

        uint32_t mask = 0;
        uint32_t values[2];

        mask = XCB_CW_OVERRIDE_REDIRECT;
        values[0] = 1;

        /* Open a new window, the resizebar. Grab the pointer and move the window around
           as the user moves the pointer. */
        Rect grabrect = {0,
                         0,
                         most_right->rect.x + most_right->rect.width,
                         most_bottom->rect.x + most_bottom->rect.height};
        xcb_window_t grabwin = create_window(conn, grabrect, XCB_WINDOW_CLASS_INPUT_ONLY, -1, true, mask, values);

        Rect helprect;
        if (orientation == O_VERTICAL) {
                helprect.x = event->root_x;
                helprect.y = screen->rect.y;
                helprect.width = 2;
                helprect.height = screen->rect.height;
                new_position = event->root_x;
        } else {
                helprect.x = screen->rect.x;
                helprect.y = event->root_y;
                helprect.width = screen->rect.width;
                helprect.height = 2;
                new_position = event->root_y;
        }

        mask = XCB_CW_BACK_PIXEL;
        values[0] = config.client.focused.border;

        mask |= XCB_CW_OVERRIDE_REDIRECT;
        values[1] = 1;

        xcb_window_t helpwin = create_window(conn, helprect, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                             (orientation == O_VERTICAL ?
                                              XCB_CURSOR_SB_H_DOUBLE_ARROW :
                                              XCB_CURSOR_SB_V_DOUBLE_ARROW), true, mask, values);

        xcb_circulate_window(conn, XCB_CIRCULATE_RAISE_LOWEST, helpwin);

        xcb_flush(conn);

        struct callback_params params = { orientation, screen, helpwin, &new_position };

        drag_pointer(conn, NULL, event, grabwin, BORDER_TOP, resize_callback, &params);

        xcb_destroy_window(conn, helpwin);
        xcb_destroy_window(conn, grabwin);
        xcb_flush(conn);

        int pixels;
        if (orientation == O_VERTICAL)
                pixels = (new_position - event->root_x);
        else pixels = (new_position - event->root_y);
        resize_container(conn, ws, first, second, orientation, pixels);

        return 1;
}

/*
 * Adjusts the container size factors according to the resizing parameters.
 * This is an abstraction used by resize_container.
 */
static void adjust_container_factors(float *factors, int ws_size, int unoccupied_size,
                int num_items, int first, int second, int pixels) {
        /* Find the current sizes */
        int sizes[num_items];
        for (int i = 0; i < num_items; ++i)
                sizes[i] = factors[i] == 0 ? ws_size / num_items : unoccupied_size * factors[i];

        /* Adjust them */
        sizes[first] += pixels;
        sizes[second] -= pixels;

        /* Calculate the new unoccupied size */
        if (factors[first] == 0) unoccupied_size += ws_size / num_items;
        if (factors[second] == 0) unoccupied_size += ws_size / num_items;

        /* Calculate the new factors */
        for (int i = 0; i < num_items; ++i) {
                if (factors[i] != 0 || i == first || i == second)
                        factors[i] = (float)sizes[i] / unoccupied_size;
        }
}

/*
 * Resizes a column/row by the given amount of pixels. Called by
 * resize_graphical_handler (the user clicked) or parse_resize_command (the
 * user issued the command)
 *
 */
void resize_container(xcb_connection_t *conn, Workspace *ws, int first, int second,
                      resize_orientation_t orientation, int pixels) {
        if (orientation == O_VERTICAL) {
                adjust_container_factors(ws->width_factor, ws->rect.width,
                                get_unoccupied_x(ws), ws->cols, first, second, pixels);
        }
        else {
                adjust_container_factors(ws->height_factor, workspace_height(ws),
                                get_unoccupied_y(ws), ws->rows, first, second, pixels);
        }

        render_layout(conn);
}
