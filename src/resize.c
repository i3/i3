/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
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
#include "xinerama.h"
#include "config.h"
#include "floating.h"

/*
 * Renders the resize window between the first/second container and resizes
 * the table column/row.
 *
 */
int resize_graphical_handler(xcb_connection_t *conn, Workspace *ws, int first, int second,
                             resize_orientation_t orientation, xcb_button_press_event_t *event) {
        int new_position;
        i3Screen *screen = get_screen_containing(event->root_x, event->root_y);
        if (screen == NULL) {
                LOG("BUG: No screen found at this position (%d, %d)\n", event->root_x, event->root_y);
                return 1;
        }

        /* We cannot use the X root window's width_in_pixels or height_in_pixels
         * attributes here since they are not updated when you configure new
         * screens during runtime. Instead, we just use the most right and most
         * bottom Xinerama screen and use their position + width/height to get
         * the area of pixels currently in use */
        i3Screen *most_right = get_screen_most(D_RIGHT, screen),
                 *most_bottom = get_screen_most(D_DOWN, screen);

        LOG("event->event_x = %d, event->root_x = %d\n", event->event_x, event->root_x);

        LOG("Screen dimensions: (%d, %d) %d x %d\n", screen->rect.x, screen->rect.y, screen->rect.width, screen->rect.height);

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
                                              XCB_CURSOR_SB_V_DOUBLE_ARROW :
                                              XCB_CURSOR_SB_H_DOUBLE_ARROW), true, mask, values);

        xcb_circulate_window(conn, XCB_CIRCULATE_RAISE_LOWEST, helpwin);

        xcb_flush(conn);

        void resize_callback(Rect *old_rect, uint32_t new_x, uint32_t new_y) {
                LOG("new x = %d, y = %d\n", new_x, new_y);
                if (orientation == O_VERTICAL) {
                        /* Check if the new coordinates are within screen boundaries */
                        if (new_x > (screen->rect.x + screen->rect.width - 25) ||
                            new_x < (screen->rect.x + 25))
                                return;

                        values[0] = new_position = new_x;
                        xcb_configure_window(conn, helpwin, XCB_CONFIG_WINDOW_X, values);
                } else {
                        if (new_y > (screen->rect.y + screen->rect.height - 25) ||
                            new_y < (screen->rect.y + 25))
                                return;

                        values[0] = new_position = new_y;
                        xcb_configure_window(conn, helpwin, XCB_CONFIG_WINDOW_Y, values);
                }

                xcb_flush(conn);
        }

        drag_pointer(conn, NULL, event, grabwin, BORDER_TOP, resize_callback);

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
 * Resizes a column/row by the given amount of pixels. Called by
 * resize_graphical_handler (the user clicked) or parse_resize_command (the
 * user issued the command)
 *
 */
void resize_container(xcb_connection_t *conn, Workspace *ws, int first, int second,
                      resize_orientation_t orientation, int pixels) {

        /* TODO: refactor this, both blocks are very identical */
        if (orientation == O_VERTICAL) {
                int default_width = ws->rect.width / ws->cols;
                int old_unoccupied_x = get_unoccupied_x(ws);

                /* We pre-calculate the unoccupied space to see if we need to adapt sizes before
                 * doing the resize */
                int new_unoccupied_x = old_unoccupied_x;

                if (old_unoccupied_x == 0)
                        old_unoccupied_x = ws->rect.width;

                if (ws->width_factor[first] == 0)
                        new_unoccupied_x += default_width;

                if (ws->width_factor[second] == 0)
                        new_unoccupied_x += default_width;

                LOG("\n\n\n");
                LOG("old = %d, new = %d\n", old_unoccupied_x, new_unoccupied_x);

                int cols_without_wf = 0;
                int old_width, old_second_width;
                for (int col = 0; col < ws->cols; col++)
                        if (ws->width_factor[col] == 0)
                                cols_without_wf++;

                LOG("old_unoccupied_x = %d\n", old_unoccupied_x);

                LOG("Updating first (before = %f)\n", ws->width_factor[first]);
                /* Convert 0 (for default width_factor) to actual numbers */
                if (ws->width_factor[first] == 0)
                        old_width = (old_unoccupied_x / max(cols_without_wf, 1));
                else old_width = ws->width_factor[first] * old_unoccupied_x;

                LOG("second (before = %f)\n", ws->width_factor[second]);
                if (ws->width_factor[second] == 0)
                        old_second_width = (old_unoccupied_x / max(cols_without_wf, 1));
                else old_second_width = ws->width_factor[second] * old_unoccupied_x;

                LOG("middle = %f\n", ws->width_factor[first]);

                /* If the space used for customly resized columns has changed we need to adapt the
                 * other customly resized columns, if any */
                if (new_unoccupied_x != old_unoccupied_x)
                        for (int col = 0; col < ws->cols; col++) {
                                if (ws->width_factor[col] == 0)
                                        continue;

                                LOG("Updating other column (%d) (current width_factor = %f)\n", col, ws->width_factor[col]);
                                ws->width_factor[col] = (ws->width_factor[col] * old_unoccupied_x) / new_unoccupied_x;
                                LOG("to %f\n", ws->width_factor[col]);
                        }

                LOG("Updating first (before = %f)\n", ws->width_factor[first]);
                /* Convert 0 (for default width_factor) to actual numbers */
                if (ws->width_factor[first] == 0)
                        ws->width_factor[first] = ((float)ws->rect.width / ws->cols) / new_unoccupied_x;

                LOG("first->width = %d, pixels = %d\n", old_width, pixels);
                ws->width_factor[first] *= (float)(old_width + pixels) / old_width;
                LOG("-> %f\n", ws->width_factor[first]);


                LOG("Updating second (before = %f)\n", ws->width_factor[second]);
                if (ws->width_factor[second] == 0)
                        ws->width_factor[second] = ((float)ws->rect.width / ws->cols) / new_unoccupied_x;

                LOG("middle = %f\n", ws->width_factor[second]);
                LOG("second->width = %d, pixels = %d\n", old_second_width, pixels);
                ws->width_factor[second] *= (float)(old_second_width - pixels) / old_second_width;
                LOG("-> %f\n", ws->width_factor[second]);

                LOG("new unoccupied_x = %d\n", get_unoccupied_x(ws));

                LOG("\n\n\n");
        } else {
                int default_height = ws->rect.height / ws->rows;
                int old_unoccupied_y = get_unoccupied_y(ws);

                /* We pre-calculate the unoccupied space to see if we need to adapt sizes before
                 * doing the resize */
                int new_unoccupied_y = old_unoccupied_y;

                if (old_unoccupied_y == 0)
                        old_unoccupied_y = ws->rect.height;

                if (ws->height_factor[first] == 0)
                        new_unoccupied_y += default_height;

                if (ws->height_factor[second] == 0)
                        new_unoccupied_y += default_height;

                LOG("\n\n\n");
                LOG("old = %d, new = %d\n", old_unoccupied_y, new_unoccupied_y);

                /* If the space used for customly resized columns has changed we need to adapt the
                 * other customly resized columns, if any */
                if (new_unoccupied_y != old_unoccupied_y)
                        for (int row = 0; row < ws->rows; row++) {
                                if (ws->height_factor[row] == 0)
                                        continue;

                                LOG("Updating other column (%d) (current width_factor = %f)\n", row, ws->height_factor[row]);
                                ws->height_factor[row] = (ws->height_factor[row] * old_unoccupied_y) / new_unoccupied_y;
                                LOG("to %f\n", ws->height_factor[row]);
                        }

                LOG("old_unoccupied_y = %d\n", old_unoccupied_y);

                LOG("Updating first (before = %f)\n", ws->height_factor[first]);
                /* Convert 0 (for default width_factor) to actual numbers */
                if (ws->height_factor[first] == 0)
                        ws->height_factor[first] = ((float)ws->rect.height / ws->rows) / new_unoccupied_y;

                LOG("middle = %f\n", ws->height_factor[first]);
                int old_height = ws->height_factor[first] * old_unoccupied_y;
                LOG("first->width = %d, pixels = %d\n", pixels);
                ws->height_factor[first] *= (float)(old_height + pixels) / old_height;
                LOG("-> %f\n", ws->height_factor[first]);


                LOG("Updating second (before = %f)\n", ws->height_factor[second]);
                if (ws->height_factor[second] == 0)
                        ws->height_factor[second] = ((float)ws->rect.height / ws->rows) / new_unoccupied_y;
                LOG("middle = %f\n", ws->height_factor[second]);
                old_height = ws->height_factor[second] * old_unoccupied_y;
                LOG("second->width = %d, pixels = %d\n", pixels);
                ws->height_factor[second] *= (float)(old_height - pixels) / old_height;
                LOG("-> %f\n", ws->height_factor[second]);

                LOG("new unoccupied_y = %d\n", get_unoccupied_y(ws));

                LOG("\n\n\n");
        }

        render_layout(conn);
}
