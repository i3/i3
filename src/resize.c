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

/*
 * Renders the resize window between the first/second container and resizes
 * the table column/row.
 *
 */
int resize_graphical_handler(xcb_connection_t *conn, Workspace *ws, int first, int second,
                             resize_orientation_t orientation, xcb_button_press_event_t *event) {
        int new_position;
        xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;
        xcb_screen_t *root_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

        /* FIXME: horizontal resizing causes empty spaces to exist */
        if (orientation == O_HORIZONTAL) {
                LOG("Sorry, horizontal resizing is not yet activated due to creating layout bugs."
                    "If you are brave, enable the code for yourself and try fixing it.\n");
                return 1;
        }

        uint32_t mask = 0;
        uint32_t values[2];

        mask = XCB_CW_OVERRIDE_REDIRECT;
        values[0] = 1;

        /* Open a new window, the resizebar. Grab the pointer and move the window around
           as the user moves the pointer. */
        Rect grabrect = {0, 0, root_screen->width_in_pixels, root_screen->height_in_pixels};
        xcb_window_t grabwin = create_window(conn, grabrect, XCB_WINDOW_CLASS_INPUT_ONLY, -1, mask, values);

        Rect helprect;
        if (orientation == O_VERTICAL) {
                helprect.x = event->root_x;
                helprect.y = 0;
                helprect.width = 2;
                helprect.height = root_screen->height_in_pixels;
                new_position = event->root_x;
        } else {
                helprect.x = 0;
                helprect.y = event->root_y;
                helprect.width = root_screen->width_in_pixels;
                helprect.height = 2;
                new_position = event->root_y;
        }

        mask = XCB_CW_BACK_PIXEL;
        values[0] = get_colorpixel(conn, "#4c7899");

        mask |= XCB_CW_OVERRIDE_REDIRECT;
        values[1] = 1;

        xcb_window_t helpwin = create_window(conn, helprect, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                             (orientation == O_VERTICAL ?
                                              XCB_CURSOR_SB_V_DOUBLE_ARROW :
                                              XCB_CURSOR_SB_H_DOUBLE_ARROW), mask, values);

        xcb_circulate_window(conn, XCB_CIRCULATE_RAISE_LOWEST, helpwin);

        xcb_grab_pointer(conn, false, root, XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION,
                        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, grabwin, XCB_NONE, XCB_CURRENT_TIME);

        xcb_flush(conn);

        xcb_generic_event_t *inside_event;
        /* I’ve always wanted to have my own eventhandler… */
        while ((inside_event = xcb_wait_for_event(conn))) {
                /* Same as get_event_handler in xcb */
                int nr = inside_event->response_type;
                if (nr == 0) {
                        /* An error occured */
                        handle_event(NULL, conn, inside_event);
                        free(inside_event);
                        continue;
                }
                assert(nr < 256);
                nr &= XCB_EVENT_RESPONSE_TYPE_MASK;
                assert(nr >= 2);

                /* Check if we need to escape this loop */
                if (nr == XCB_BUTTON_RELEASE)
                        break;

                switch (nr) {
                        case XCB_MOTION_NOTIFY:
                                if (orientation == O_VERTICAL) {
                                        values[0] = new_position = ((xcb_motion_notify_event_t*)inside_event)->root_x;
                                        xcb_configure_window(conn, helpwin, XCB_CONFIG_WINDOW_X, values);
                                } else {
                                        values[0] = new_position = ((xcb_motion_notify_event_t*)inside_event)->root_y;
                                        xcb_configure_window(conn, helpwin, XCB_CONFIG_WINDOW_Y, values);
                                }

                                xcb_flush(conn);
                                break;
                        default:
                                LOG("Passing to original handler\n");
                                /* Use original handler */
                                xcb_event_handle(&evenths, inside_event);
                                break;
                }
                free(inside_event);
        }

        xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
        xcb_destroy_window(conn, helpwin);
        xcb_destroy_window(conn, grabwin);
        xcb_flush(conn);

        if (orientation == O_VERTICAL) {
                LOG("Resize was from X = %d to X = %d\n", event->root_x, new_position);
                if (event->root_x == new_position) {
                        LOG("Nothing changed, not updating anything\n");
                        return 1;
                }

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

                LOG("old_unoccupied_x = %d\n", old_unoccupied_x);

                LOG("Updating first (before = %f)\n", ws->width_factor[first]);
                /* Convert 0 (for default width_factor) to actual numbers */
                if (ws->width_factor[first] == 0)
                        ws->width_factor[first] = ((float)ws->rect.width / ws->cols) / new_unoccupied_x;

                LOG("middle = %f\n", ws->width_factor[first]);
                int old_width = ws->width_factor[first] * old_unoccupied_x;
                LOG("first->width = %d, new_position = %d, event->root_x = %d\n", old_width, new_position, event->root_x);
                ws->width_factor[first] *= (float)(old_width + (new_position - event->root_x)) / old_width;
                LOG("-> %f\n", ws->width_factor[first]);


                LOG("Updating second (before = %f)\n", ws->width_factor[second]);
                if (ws->width_factor[second] == 0)
                        ws->width_factor[second] = ((float)ws->rect.width / ws->cols) / new_unoccupied_x;
                LOG("middle = %f\n", ws->width_factor[second]);
                old_width = ws->width_factor[second] * old_unoccupied_x;
                LOG("second->width = %d, new_position = %d, event->root_x = %d\n", old_width, new_position, event->root_x);
                ws->width_factor[second] *= (float)(old_width - (new_position - event->root_x)) / old_width;
                LOG("-> %f\n", ws->width_factor[second]);

                LOG("new unoccupied_x = %d\n", get_unoccupied_x(ws));

                LOG("\n\n\n");
        } else {
#if 0
                LOG("Resize was from Y = %d to Y = %d\n", event->root_y, new_position);
                if (event->root_y == new_position) {
                        LOG("Nothing changed, not updating anything\n");
                        return 1;
                }

                /* Convert 0 (for default height_factor) to actual numbers */
                if (first->height_factor == 0)
                        first->height_factor = ((float)ws->rect.height / ws->rows) / ws->rect.height;
                if (second->height_factor == 0)
                        second->height_factor = ((float)ws->rect.height / ws->rows) / ws->rect.height;

                first->height_factor *= (float)(first->height + (new_position - event->root_y)) / first->height;
                second->height_factor *= (float)(second->height - (new_position - event->root_y)) / second->height;
#endif
        }

        render_layout(conn);

        return 1;
}
