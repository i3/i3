/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * layout.c: Functions handling layout/drawing of window decorations
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <assert.h>
#include <math.h>

#include "config.h"
#include "i3.h"
#include "xcb.h"
#include "table.h"
#include "util.h"
#include "xinerama.h"
#include "layout.h"

/*
 * Updates *destination with new_value and returns true if it was changed or false
 * if it was the same
 *
 */
static bool update_if_necessary(uint32_t *destination, const uint32_t new_value) {
        int old_value = *destination;

        return ((*destination = new_value) != old_value);
}

/*
 * Gets the unoccupied space (= space which is available for windows which were resized by the user)
 * for the given row. This is necessary to render both, customly resized windows and never touched
 * windows correctly, meaning that the aspect ratio will be maintained when opening new windows.
 *
 */
int get_unoccupied_x(Workspace *workspace, int row) {
        int unoccupied = workspace->rect.width;
        float default_factor = ((float)workspace->rect.width / workspace->cols) / workspace->rect.width;

        LOG("get_unoccupied_x(), starting with %d, default_factor = %f\n", unoccupied, default_factor);

        for (int cols = 0; cols < workspace->cols;) {
                Container *con = workspace->table[cols][row];
                LOG("width_factor[%d][%d] = %f, colspan = %d\n", cols, row, con->width_factor, con->colspan);
                if (con->width_factor == 0)
                        unoccupied -= workspace->rect.width * default_factor * con->colspan;
                cols += con->colspan;
        }

        assert(unoccupied != 0);

        LOG("unoccupied space: %d\n", unoccupied);
        return unoccupied;
}

/* See get_unoccupied_x() */
int get_unoccupied_y(Workspace *workspace, int col) {
        int unoccupied = workspace->rect.height;
        float default_factor = ((float)workspace->rect.height / workspace->rows) / workspace->rect.height;

        LOG("get_unoccupied_y(), starting with %d, default_factor = %f\n", unoccupied, default_factor);

        for (int rows = 0; rows < workspace->rows;) {
                Container *con = workspace->table[col][rows];
                LOG("height_factor[%d][%d] = %f, rowspan %d\n", col, rows, con->height_factor, con->rowspan);
                if (con->height_factor == 0)
                        unoccupied -= workspace->rect.height * default_factor * con->rowspan;
                rows += con->rowspan;
        }

        assert(unoccupied != 0);

        LOG("unoccupied space: %d\n", unoccupied);
        return unoccupied;
}

/*
 * Redecorates the given client correctly by checking if it’s in a stacking container and
 * re-rendering the stack window or just calling decorate_window if it’s not in a stacking
 * container.
 *
 */
void redecorate_window(xcb_connection_t *conn, Client *client) {
        if (client->container->mode == MODE_STACK) {
                render_container(conn, client->container);
                /* We clear the frame to generate exposure events, because the color used
                   in drawing may be different */
                xcb_clear_area(conn, true, client->frame, 0, 0, client->rect.width, client->rect.height);
        } else decorate_window(conn, client, client->frame, client->titlegc, 0);
        xcb_flush(conn);
}

/*
 * (Re-)draws window decorations for a given Client onto the given drawable/graphic context.
 * When in stacking mode, the window decorations are drawn onto an own window.
 *
 */
void decorate_window(xcb_connection_t *conn, Client *client, xcb_drawable_t drawable, xcb_gcontext_t gc, int offset) {
        i3Font *font = load_font(conn, config.font);
        int decoration_height = font->height + 2 + 2;
        uint32_t background_color,
                 text_color,
                 border_color;

        /* Clients without a container (docks) won’t get decorated */
        if (client->container == NULL)
                return;

        if (client->container->currently_focused == client) {
                /* Distinguish if the window is currently focused… */
                if (CUR_CELL->currently_focused == client)
                        background_color = get_colorpixel(conn, "#285577");
                /* …or if it is the focused window in a not focused container */
                else background_color = get_colorpixel(conn, "#555555");

                text_color = get_colorpixel(conn, "#ffffff");
                border_color = get_colorpixel(conn, "#4c7899");
        } else {
                background_color = get_colorpixel(conn, "#222222");
                text_color = get_colorpixel(conn, "#888888");
                border_color = get_colorpixel(conn, "#333333");
        }

        /* Our plan is the following:
           - Draw a rect around the whole client in background_color
           - Draw two lines in a lighter color
           - Draw the window’s title
         */

        /* Draw a rectangle in background color around the window */
        xcb_change_gc_single(conn, gc, XCB_GC_FOREGROUND, background_color);

        /* In stacking mode, we only render the rect for this specific decoration */
        if (client->container->mode == MODE_STACK) {
                xcb_rectangle_t rect = {0, offset, client->container->width, offset + decoration_height };
                xcb_poly_fill_rectangle(conn, drawable, gc, 1, &rect);
        } else {
                /* We need to use the container’s width because it is the more recent value - when
                   in stacking mode, clients get reconfigured only on demand (the not active client
                   is not reconfigured), so the client’s rect.width would be wrong */
                xcb_rectangle_t rect = {0, 0, client->container->width, client->rect.height};
                xcb_poly_fill_rectangle(conn, drawable, gc, 1, &rect);

                /* Draw the inner background to have a black frame around clients (such as mplayer)
                   which cannot be resized exactly in our frames and therefore are centered */
                xcb_change_gc_single(conn, client->titlegc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#000000"));
                xcb_rectangle_t crect = {2, decoration_height,
                                         client->rect.width - (2 + 2), client->rect.height - 2 - decoration_height};
                xcb_poly_fill_rectangle(conn, client->frame, client->titlegc, 1, &crect);
        }

        /* Draw the lines */
        xcb_draw_line(conn, drawable, gc, border_color, 2, offset, client->rect.width, offset);
        xcb_draw_line(conn, drawable, gc, border_color, 2, offset + font->height + 3,
                      2 + client->rect.width, offset + font->height + 3);

        /* If the client has a title, we draw it */
        if (client->name != NULL) {
                /* Draw the font */
                uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
                uint32_t values[] = { text_color, background_color, font->id };
                xcb_change_gc(conn, gc, mask, values);

                /* name_len == -1 means this is a legacy application which does not specify _NET_WM_NAME,
                   and we don’t handle the old window name (COMPOUND_TEXT) but only _NET_WM_NAME, which
                   is UTF-8 */
                if (client->name_len == -1)
                        xcb_image_text_8(conn, strlen(client->name), drawable, gc, 3 /* X */,
                                         offset + font->height /* Y = baseline of font */, client->name);
                else
                        xcb_image_text_16(conn, client->name_len, drawable, gc, 3 /* X */,
                                          offset + font->height /* Y = baseline of font */, (xcb_char2b_t*)client->name);
        }
}

/*
 * Pushes the client’s x and y coordinates to X11
 *
 */
static void reposition_client(xcb_connection_t *conn, Client *client) {
        LOG("frame 0x%08x needs to be pushed to %dx%d\n", client->frame, client->rect.x, client->rect.y);
        /* Note: We can use a pointer to client->x like an array of uint32_ts
           because it is followed by client->y by definition */
        xcb_configure_window(conn, client->frame, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, &(client->rect.x));
}

/*
 * Pushes the client’s width/height to X11 and resizes the child window
 *
 */
static void resize_client(xcb_connection_t *conn, Client *client) {
        i3Font *font = load_font(conn, config.font);

        LOG("resizing client 0x%08x to %d x %d\n", client->frame, client->rect.width, client->rect.height);
        xcb_configure_window(conn, client->frame,
                        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                        &(client->rect.width));

        /* Adjust the position of the child inside its frame.
         * The coordinates of the child are relative to its frame, we
         * add a border of 2 pixel to each value */
        uint32_t mask = XCB_CONFIG_WINDOW_X |
                        XCB_CONFIG_WINDOW_Y |
                        XCB_CONFIG_WINDOW_WIDTH |
                        XCB_CONFIG_WINDOW_HEIGHT;
        Rect *rect = &(client->child_rect);
        switch ((client->container != NULL ? client->container->mode : MODE_DEFAULT)) {
                case MODE_STACK:
                        rect->x = 2;
                        rect->y = 0;
                        rect->width = client->rect.width - (2 + 2);
                        rect->height = client->rect.height - 2;
                        break;
                default:
                        if (client->titlebar_position == TITLEBAR_OFF) {
                                rect->x = 0;
                                rect->y = 0;
                                rect->width = client->rect.width;
                                rect->height = client->rect.height;
                        } else {
                                rect->x = 2;
                                rect->y = font->height + 2 + 2;
                                rect->width = client->rect.width - (2 + 2);
                                rect->height = client->rect.height - ((font->height + 2 + 2) + 2);
                        }
                        break;
        }

        /* Obey the ratio, if any */
        if (client->proportional_height != 0 &&
            client->proportional_width != 0) {
                LOG("proportional height = %d, width = %d\n", client->proportional_height, client->proportional_width);
                double new_height = rect->height + 1;
                int new_width = rect->width;

                while (new_height > rect->height) {
                        new_height = ((double)client->proportional_height / client->proportional_width) * new_width;

                        if (new_height > rect->height)
                                new_width--;
                }
                /* Center the window */
                rect->y += ceil(rect->height / 2) - floor(new_height / 2);
                rect->x += ceil(rect->width / 2) - floor(new_width / 2);

                rect->height = new_height;
                rect->width = new_width;
                LOG("new_height = %f, new_width = %d\n", new_height, new_width);
        }

        LOG("child will be at %dx%d with size %dx%d\n", rect->x, rect->y, rect->width, rect->height);

        xcb_configure_window(conn, client->child, mask, &(rect->x));

        /* After configuring a child window we need to fake a configure_notify_event according
           to ICCCM 4.2.3. This seems rather broken, especially since X sends exactly the same
           configure_notify_event automatically according to xtrace. Anyone knows details? */
        fake_configure_notify(conn, *rect, client->child);
}

/*
 * Renders the given container. Is called by render_layout() or individually (for example
 * when focus changes in a stacking container)
 *
 */
void render_container(xcb_connection_t *conn, Container *container) {
        Client *client;
        int num_clients = 0, current_client = 0;

        if (container->currently_focused == NULL)
                return;

        CIRCLEQ_FOREACH(client, &(container->clients), clients)
                num_clients++;

        if (container->mode == MODE_DEFAULT) {
                LOG("got %d clients in this default container.\n", num_clients);
                CIRCLEQ_FOREACH(client, &(container->clients), clients) {
                        /* If the client is in fullscreen mode, it does not get reconfigured */
                        if (container->workspace->fullscreen_client == client) {
                                current_client++;
                                continue;
                        }

                        /* Check if we changed client->x or client->y by updating it.
                         * Note the bitwise OR instead of logical OR to force evaluation of both statements */
                        if (client->force_reconfigure |
                            update_if_necessary(&(client->rect.x), container->x) |
                            update_if_necessary(&(client->rect.y), container->y +
                                        (container->height / num_clients) * current_client))
                                reposition_client(conn, client);

                        /* TODO: vertical default layout */
                        if (client->force_reconfigure |
                            update_if_necessary(&(client->rect.width), container->width) |
                            update_if_necessary(&(client->rect.height), container->height / num_clients))
                                resize_client(conn, client);

                        client->force_reconfigure = false;

                        current_client++;
                }
        } else {
                i3Font *font = load_font(conn, config.font);
                int decoration_height = (font->height + 2 + 2);
                struct Stack_Window *stack_win = &(container->stack_win);

                /* Check if we need to remap our stack title window, it gets unmapped when the container
                   is empty in src/handlers.c:unmap_notify() */
                if (stack_win->rect.height == 0)
                        xcb_map_window(conn, stack_win->window);

                /* Check if we need to reconfigure our stack title window */
                if (update_if_necessary(&(stack_win->rect.x), container->x) |
                    update_if_necessary(&(stack_win->rect.y), container->y) |
                    update_if_necessary(&(stack_win->rect.width), container->width) |
                    update_if_necessary(&(stack_win->rect.height), decoration_height * num_clients)) {

                        /* Configuration can happen in two slightly different ways:

                           If there is no client in fullscreen mode, 5 parameters are passed
                           (x, y, width, height, stack mode is set to above which means top-most position).

                           If there is a fullscreen client, the fourth parameter is set to to the
                           fullscreen window as sibling and the stack mode is set to below, which means
                           that the stack_window will be placed just below the sibling, that is, under
                           the fullscreen window.
                         */
                        uint32_t values[] = { stack_win->rect.x, stack_win->rect.y,
                                              stack_win->rect.width, stack_win->rect.height,
                                              XCB_STACK_MODE_ABOVE, XCB_STACK_MODE_BELOW };
                        uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                                        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                                        XCB_CONFIG_WINDOW_STACK_MODE;

                        /* If there is no fullscreen client, we raise the stack window */
                        if (container->workspace->fullscreen_client != NULL) {
                                mask |= XCB_CONFIG_WINDOW_SIBLING;
                                values[4] = container->workspace->fullscreen_client->frame;
                        }

                        xcb_configure_window(conn, stack_win->window, mask, values);
                }

                /* Reconfigure the currently focused client, if necessary. It is the only visible one */
                client = container->currently_focused;

                if (container->workspace->fullscreen_client == NULL) {
                        uint32_t values[] = { XCB_STACK_MODE_ABOVE };
                        xcb_configure_window(conn, client->frame, XCB_CONFIG_WINDOW_STACK_MODE, values);
                }

                /* Render the decorations of all clients */
                CIRCLEQ_FOREACH(client, &(container->clients), clients) {
                        /* If the client is in fullscreen mode, it does not get reconfigured */
                        if (container->workspace->fullscreen_client == client) {
                                current_client++;
                                continue;
                        }

                        /* Check if we changed client->x or client->y by updating it.
                         * Note the bitwise OR instead of logical OR to force evaluation of both statements */
                        if (client->force_reconfigure |
                            update_if_necessary(&(client->rect.x), container->x) |
                            update_if_necessary(&(client->rect.y), container->y + (decoration_height * num_clients)))
                                reposition_client(conn, client);

                        if (client->force_reconfigure |
                            update_if_necessary(&(client->rect.width), container->width) |
                            update_if_necessary(&(client->rect.height), container->height - (decoration_height * num_clients)))
                                resize_client(conn, client);

                        client->force_reconfigure = false;

                        decorate_window(conn, client, stack_win->window, stack_win->gc,
                                        current_client++ * decoration_height);
                }
        }
}

static void render_bars(xcb_connection_t *conn, Workspace *r_ws, int width, int *height) {
        Client *client;
        SLIST_FOREACH(client, &(r_ws->screen->dock_clients), dock_clients) {
                LOG("client is at %d, should be at %d\n", client->rect.y, *height);
                if (client->force_reconfigure |
                    update_if_necessary(&(client->rect.x), 0) |
                    update_if_necessary(&(client->rect.y), *height))
                        reposition_client(conn, client);

                if (client->force_reconfigure |
                    update_if_necessary(&(client->rect.width), width) |
                    update_if_necessary(&(client->rect.height), client->desired_height))
                        resize_client(conn, client);

                client->force_reconfigure = false;
                LOG("desired_height = %d\n", client->desired_height);
                *height += client->desired_height;
        }
}

static void render_internal_bar(xcb_connection_t *conn, Workspace *r_ws, int width, int height) {
        LOG("Rendering internal bar\n");
        i3Font *font = load_font(conn, config.font);
        i3Screen *screen = r_ws->screen;
        enum { SET_NORMAL = 0, SET_FOCUSED = 1 };
        uint32_t background_color[2],
                 text_color[2],
                 border_color[2],
                 black;
        char label[3];

        black = get_colorpixel(conn, "#000000");

        background_color[SET_NORMAL] = get_colorpixel(conn, "#222222");
        text_color[SET_NORMAL] = get_colorpixel(conn, "#888888");
        border_color[SET_NORMAL] = get_colorpixel(conn, "#333333");

        background_color[SET_FOCUSED] = get_colorpixel(conn, "#285577");
        text_color[SET_FOCUSED] = get_colorpixel(conn, "#ffffff");
        border_color[SET_FOCUSED] = get_colorpixel(conn, "#4c7899");

        /* Fill the whole bar in black */
        xcb_change_gc_single(conn, screen->bargc, XCB_GC_FOREGROUND, black);
        xcb_rectangle_t rect = {0, 0, width, height};
        xcb_poly_fill_rectangle(conn, screen->bar, screen->bargc, 1, &rect);

        /* Set font */
        xcb_change_gc_single(conn, screen->bargc, XCB_GC_FONT, font->id);

        int drawn = 0;
        for (int c = 0; c < 10; c++) {
                if (workspaces[c].screen != screen)
                        continue;

                int set = (screen->current_workspace == c ? SET_FOCUSED : SET_NORMAL);

                xcb_draw_rect(conn, screen->bar, screen->bargc, border_color[set],
                              drawn * height, 1, height - 2, height - 2);
                xcb_draw_rect(conn, screen->bar, screen->bargc, background_color[set],
                              drawn * height + 1, 2, height - 4, height - 4);

                snprintf(label, sizeof(label), "%d", c+1);
                xcb_change_gc_single(conn, screen->bargc, XCB_GC_FOREGROUND, text_color[set]);
                xcb_change_gc_single(conn, screen->bargc, XCB_GC_BACKGROUND, background_color[set]);
                xcb_image_text_8(conn, strlen(label), screen->bar, screen->bargc, drawn * height + 5 /* X */,
                                                font->height + 1 /* Y = baseline of font */, label);
                drawn++;
        }

        LOG("done rendering internal\n");
}

/*
 * Modifies the event mask of all clients on the given workspace to either ignore or to handle
 * enter notifies. It is handy to ignore notifies because they will be sent when a window is mapped
 * under the cursor, thus when the user didn’t enter the window actively at all.
 *
 */
void ignore_enter_notify_forall(xcb_connection_t *conn, Workspace *workspace, bool ignore_enter_notify) {
        Client *client;
        uint32_t values[1];

        LOG("Ignore enter_notify = %d\n", ignore_enter_notify);

        FOR_TABLE(workspace)
                CIRCLEQ_FOREACH(client, &(workspace->table[cols][rows]->clients), clients) {
                        /* Change event mask for the decorations */
                        values[0] = FRAME_EVENT_MASK;
                        if (ignore_enter_notify)
                                values[0] &= ~(XCB_EVENT_MASK_ENTER_WINDOW);
                        xcb_change_window_attributes(conn, client->frame, XCB_CW_EVENT_MASK, values);

                        /* Change event mask for the child itself */
                        values[0] = CHILD_EVENT_MASK;
                        if (ignore_enter_notify)
                                values[0] &= ~(XCB_EVENT_MASK_ENTER_WINDOW);
                        xcb_change_window_attributes(conn, client->child, XCB_CW_EVENT_MASK, values);
                }
}

/*
 * Renders the whole layout, that is: Go through each screen, each workspace, each container
 * and render each client. This also renders the bars.
 *
 * If you don’t need to render *everything*, you should call render_container on the container
 * you want to refresh.
 *
 */
void render_layout(xcb_connection_t *conn) {
        i3Screen *screen;
        i3Font *font = load_font(conn, config.font);

        TAILQ_FOREACH(screen, virtual_screens, screens) {
                /* r_ws (rendering workspace) is just a shortcut to the Workspace being currently rendered */
                Workspace *r_ws = &(workspaces[screen->current_workspace]);

                LOG("Rendering screen %d\n", screen->num);

                int width = r_ws->rect.width;
                int height = r_ws->rect.height;

                /* Reserve space for dock clients */
                Client *client;
                SLIST_FOREACH(client, &(screen->dock_clients), dock_clients)
                        height -= client->desired_height;

                /* Space for the internal bar */
                height -= (font->height + 6);

                LOG("got %d rows and %d cols\n", r_ws->rows, r_ws->cols);

                int xoffset[r_ws->rows];
                int yoffset[r_ws->cols];
                /* Initialize offsets */
                for (int cols = 0; cols < r_ws->cols; cols++)
                        yoffset[cols] = r_ws->rect.y;
                for (int rows = 0; rows < r_ws->rows; rows++)
                        xoffset[rows] = r_ws->rect.x;

                dump_table(conn, r_ws);

                ignore_enter_notify_forall(conn, r_ws, true);

                /* Go through the whole table and render what’s necessary */
                FOR_TABLE(r_ws) {
                        Container *container = r_ws->table[cols][rows];
                        int single_width, single_height;
                        LOG("\n");
                        LOG("========\n");
                        LOG("container has %d colspan, %d rowspan\n",
                                        container->colspan, container->rowspan);
                        LOG("container at %d, %d\n", xoffset[rows], yoffset[cols]);
                        /* Update position of the container */
                        container->row = rows;
                        container->col = cols;
                        container->x = xoffset[rows];
                        container->y = yoffset[cols];

                        if (container->width_factor == 0)
                                container->width = (width / r_ws->cols);
                        else container->width = get_unoccupied_x(r_ws, rows) * container->width_factor;
                        single_width = container->width;
                        container->width *= container->colspan;

                        if (container->height_factor == 0)
                                container->height = (height / r_ws->rows);
                        else container->height = get_unoccupied_y(r_ws, cols) * container->height_factor;
                        single_height = container->height;
                        container->height *= container->rowspan;

                        /* Render the container if it is not empty */
                        render_container(conn, container);

                        xoffset[rows] += single_width;
                        yoffset[cols] += single_height;
                        LOG("==========\n");
                }

                ignore_enter_notify_forall(conn, r_ws, false);

                render_bars(conn, r_ws, width, &height);
                render_internal_bar(conn, r_ws, width, font->height + 6);
        }

        xcb_flush(conn);
}
