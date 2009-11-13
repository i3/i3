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
#include "client.h"
#include "floating.h"
#include "handlers.h"
#include "workspace.h"

/*
 * Updates *destination with new_value and returns true if it was changed or false
 * if it was the same
 *
 */
static bool update_if_necessary(uint32_t *destination, const uint32_t new_value) {
        uint32_t old_value = *destination;

        return ((*destination = new_value) != old_value);
}

/*
 * Gets the unoccupied space (= space which is available for windows which were resized by the user)
 * for the given row. This is necessary to render both, customly resized windows and never touched
 * windows correctly, meaning that the aspect ratio will be maintained when opening new windows.
 *
 */
int get_unoccupied_x(Workspace *workspace) {
        int unoccupied = workspace->rect.width;
        float default_factor = ((float)workspace->rect.width / workspace->cols) / workspace->rect.width;

        LOG("get_unoccupied_x(), starting with %d, default_factor = %f\n", unoccupied, default_factor);

        for (int cols = 0; cols < workspace->cols; cols++) {
                LOG("width_factor[%d] = %f\n", cols, workspace->width_factor[cols]);

                if (workspace->width_factor[cols] == 0)
                        unoccupied -= workspace->rect.width * default_factor;
        }

        LOG("unoccupied space: %d\n", unoccupied);
        return unoccupied;
}

/* See get_unoccupied_x() */
int get_unoccupied_y(Workspace *workspace) {
        int height = workspace->rect.height;
        i3Font *font = load_font(global_conn, config.font);

        /* Reserve space for dock clients */
        Client *client;
        SLIST_FOREACH(client, &(workspace->screen->dock_clients), dock_clients)
                height -= client->desired_height;

        /* Space for the internal bar */
        height -= (font->height + 6);

        int unoccupied = height;
        float default_factor = ((float)height / workspace->rows) / height;

        LOG("get_unoccupied_y(), starting with %d, default_factor = %f\n", unoccupied, default_factor);

        for (int rows = 0; rows < workspace->rows; rows++) {
                LOG("height_factor[%d] = %f\n", rows, workspace->height_factor[rows]);
                if (workspace->height_factor[rows] == 0)
                        unoccupied -= height * default_factor;
        }

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
        if (client->container != NULL &&
            (client->container->mode == MODE_STACK ||
             client->container->mode == MODE_TABBED)) {
                render_container(conn, client->container);
                /* We clear the frame to generate exposure events, because the color used
                   in drawing may be different */
                xcb_clear_area(conn, true, client->frame, 0, 0, client->rect.width, client->rect.height);
        } else decorate_window(conn, client, client->frame, client->titlegc, 0, 0);
        xcb_flush(conn);
}

/*
 * (Re-)draws window decorations for a given Client onto the given drawable/graphic context.
 * When in stacking mode, the window decorations are drawn onto an own window.
 *
 */
void decorate_window(xcb_connection_t *conn, Client *client, xcb_drawable_t drawable,
                     xcb_gcontext_t gc, int offset_x, int offset_y) {
        i3Font *font = load_font(conn, config.font);
        int decoration_height = font->height + 2 + 2;
        struct Colortriple *color;
        Client *last_focused;

        /* Clients without a container (docks) won’t get decorated */
        if (client->dock)
                return;

        last_focused = SLIST_FIRST(&(client->workspace->focus_stack));
        /* Is the window urgent? */
        if (client->urgent)
                color = &(config.client.urgent);
        else {
                if (client_is_floating(client)) {
                        if (last_focused == client)
                                color = &(config.client.focused);
                        else color = &(config.client.unfocused);
                } else {
                        if (client->container->currently_focused == client) {
                                /* Distinguish if the window is currently focused… */
                                if (last_focused == client && c_ws == client->workspace)
                                        color = &(config.client.focused);
                                /* …or if it is the focused window in a not focused container */
                                else color = &(config.client.focused_inactive);
                        } else color = &(config.client.unfocused);
                }
        }

        /* Our plan is the following:
           - Draw a rect around the whole client in color->background
           - Draw two lines in a lighter color
           - Draw the window’s title
         */

        /* Draw a rectangle in background color around the window */
        if (client->borderless && (client->container == NULL ||
            (client->container->mode != MODE_STACK &&
             client->container->mode != MODE_TABBED)))
                xcb_change_gc_single(conn, gc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#000000"));
        else xcb_change_gc_single(conn, gc, XCB_GC_FOREGROUND, color->background);

        /* In stacking mode, we only render the rect for this specific decoration */
        if (client->container != NULL && (client->container->mode == MODE_STACK || client->container->mode == MODE_TABBED)) {
                /* We need to use the container’s width because it is the more recent value - when
                   in stacking mode, clients get reconfigured only on demand (the not active client
                   is not reconfigured), so the client’s rect.width would be wrong */
                xcb_rectangle_t rect = {offset_x, offset_y,
                                        offset_x + client->container->width,
                                        offset_y + decoration_height };
                xcb_poly_fill_rectangle(conn, drawable, gc, 1, &rect);
        } else {
                xcb_rectangle_t rect = {0, 0, client->rect.width, client->rect.height};
                xcb_poly_fill_rectangle(conn, drawable, gc, 1, &rect);

                /* Draw the inner background to have a black frame around clients (such as mplayer)
                   which cannot be resized exactly in our frames and therefore are centered */
                xcb_change_gc_single(conn, client->titlegc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#000000"));
                if (client->titlebar_position == TITLEBAR_OFF) {
                        xcb_rectangle_t crect = {1, 1, client->rect.width - (1 + 1), client->rect.height - (1 + 1)};
                        xcb_poly_fill_rectangle(conn, client->frame, client->titlegc, 1, &crect);
                } else {
                        xcb_rectangle_t crect = {2, decoration_height,
                                                 client->rect.width - (2 + 2), client->rect.height - 2 - decoration_height};
                        xcb_poly_fill_rectangle(conn, client->frame, client->titlegc, 1, &crect);
                }
        }

        if (client->titlebar_position != TITLEBAR_OFF) {
                /* Draw the lines */
                xcb_draw_line(conn, drawable, gc, color->border, offset_x, offset_y, offset_x + client->rect.width, offset_y);
                if ((client->container == NULL ||
                    (client->container->mode != MODE_STACK &&
                     client->container->mode != MODE_TABBED) ||
                    CIRCLEQ_NEXT_OR_NULL(&(client->container->clients), client, clients) == NULL))
                        xcb_draw_line(conn, drawable, gc, color->border,
                                      offset_x + 2, /* x */
                                      offset_y + font->height + 3, /* y */
                                      offset_x + client->rect.width - 3, /* to_x */
                                      offset_y + font->height + 3 /* to_y */);
        }

        /* If the client has a title, we draw it */
        if (client->name != NULL && client->titlebar_position != TITLEBAR_OFF) {
                /* Draw the font */
                uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
                uint32_t values[] = { color->text, color->background, font->id };
                xcb_change_gc(conn, gc, mask, values);

                /* name_len == -1 means this is a legacy application which does not specify _NET_WM_NAME,
                   and we don’t handle the old window name (COMPOUND_TEXT) but only _NET_WM_NAME, which
                   is UTF-8 */
                if (client->name_len == -1)
                        xcb_image_text_8(conn, strlen(client->name), drawable, gc, offset_x + 3 /* X */,
                                         offset_y + font->height /* Y = baseline of font */, client->name);
                else
                        xcb_image_text_16(conn, client->name_len, drawable, gc, offset_x + 3 /* X */,
                                          offset_y + font->height /* Y = baseline of font */, (xcb_char2b_t*)client->name);
        }
}

/*
 * Pushes the client’s x and y coordinates to X11
 *
 */
void reposition_client(xcb_connection_t *conn, Client *client) {
        i3Screen *screen;

        LOG("frame 0x%08x needs to be pushed to %dx%d\n", client->frame, client->rect.x, client->rect.y);
        /* Note: We can use a pointer to client->x like an array of uint32_ts
           because it is followed by client->y by definition */
        xcb_configure_window(conn, client->frame, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, &(client->rect.x));

        if (!client_is_floating(client))
                return;

        /* If the client is floating, we need to check if we moved it to a different workspace */
        if (client->workspace->screen == (screen = get_screen_containing(client->rect.x, client->rect.y)))
                return;

        if (screen == NULL) {
                LOG("Boundary checking disabled, no screen found for (%d, %d)\n", client->rect.x, client->rect.y);
                return;
        }

        LOG("Client is on workspace %p with screen %p\n", client->workspace, client->workspace->screen);
        LOG("but screen at %d, %d is %p\n", client->rect.x, client->rect.y, screen);
        floating_assign_to_workspace(client, screen->current_workspace);
}

/*
 * Pushes the client’s width/height to X11 and resizes the child window. This
 * function also updates the client’s position, so if you work on tiling clients
 * only, you can use this function instead of separate calls to reposition_client
 * and resize_client to reduce flickering.
 *
 */
void resize_client(xcb_connection_t *conn, Client *client) {
        i3Font *font = load_font(conn, config.font);

        LOG("frame 0x%08x needs to be pushed to %dx%d\n", client->frame, client->rect.x, client->rect.y);
        LOG("resizing client 0x%08x to %d x %d\n", client->frame, client->rect.width, client->rect.height);
        xcb_configure_window(conn, client->frame,
                        XCB_CONFIG_WINDOW_X |
                        XCB_CONFIG_WINDOW_Y |
                        XCB_CONFIG_WINDOW_WIDTH |
                        XCB_CONFIG_WINDOW_HEIGHT,
                        &(client->rect.x));

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
                case MODE_TABBED:
                        rect->x = 2;
                        rect->y = 0;
                        rect->width = client->rect.width - (2 + 2);
                        rect->height = client->rect.height - 2;
                        break;
                default:
                        if (client->titlebar_position == TITLEBAR_OFF && client->borderless) {
                                rect->x = 0;
                                rect->y = 0;
                                rect->width = client->rect.width;
                                rect->height = client->rect.height;
                        } else if (client->titlebar_position == TITLEBAR_OFF && !client->borderless) {
                                rect->x = 1;
                                rect->y = 1;
                                rect->width = client->rect.width - 1 - 1;
                                rect->height = client->rect.height - 1 - 1;
                        } else {
                                rect->x = 2;
                                rect->y = font->height + 2 + 2;
                                rect->width = client->rect.width - (2 + 2);
                                rect->height = client->rect.height - ((font->height + 2 + 2) + 2);
                        }
                        break;
        }

        rect->width -= (2 * client->border_width);
        rect->height -= (2 * client->border_width);

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

        if (client->height_increment > 1) {
                int old_height = rect->height;
                rect->height -= (rect->height - client->base_height) % client->height_increment;
                LOG("Lost %d pixel due to client's height_increment (%d px)\n",
                    old_height - rect->height, client->height_increment);
        }

        if (client->width_increment > 1) {
                int old_width = rect->width;
                rect->width -= (rect->width - client->base_width) % client->width_increment;
                LOG("Lost %d pixel due to client's width_increment (%d px)\n",
                    old_width - rect->width, client->width_increment);
        }

        LOG("child will be at %dx%d with size %dx%d\n", rect->x, rect->y, rect->width, rect->height);

        xcb_configure_window(conn, client->child, mask, &(rect->x));

        /* After configuring a child window we need to fake a configure_notify_event (see ICCCM 4.2.3).
         * This is necessary to inform the client of its position relative to the root window,
         * not relative to its frame (as done in the configure_notify_event by the x server). */
        fake_absolute_configure_notify(conn, client);

        /* Force redrawing after resizing the window because any now lost
         * pixels could contain old garbage. */
        xcb_expose_event_t generated;
        generated.window = client->frame;
        generated.count = 0;
        handle_expose_event(NULL, conn, &generated);
}

/*
 * Renders the given container. Is called by render_layout() or individually (for example
 * when focus changes in a stacking container)
 *
 */
void render_container(xcb_connection_t *conn, Container *container) {
        Client *client;
        int num_clients = 0, current_client = 0;

        CIRCLEQ_FOREACH(client, &(container->clients), clients)
                num_clients++;

        if (container->mode == MODE_DEFAULT) {
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
                                        (container->height / num_clients) * current_client) |
                            update_if_necessary(&(client->rect.width), container->width) |
                            update_if_necessary(&(client->rect.height), container->height / num_clients))
                                resize_client(conn, client);

                        /* TODO: vertical default layout */

                        client->force_reconfigure = false;

                        current_client++;
                }
        } else {
                i3Font *font = load_font(conn, config.font);
                int decoration_height = (font->height + 2 + 2);
                struct Stack_Window *stack_win = &(container->stack_win);
                /* The size for each tab (width), necessary as a separate variable
                 * because num_clients gets fixed to 1 in tabbed mode. */
                int size_each = (num_clients == 0 ? container->width : container->width / num_clients);
                int stack_lines = num_clients;

                /* Check if we need to remap our stack title window, it gets unmapped when the container
                   is empty in src/handlers.c:unmap_notify() */
                if (stack_win->rect.height == 0 && num_clients > 0) {
                        LOG("remapping stack win\n");
                        xcb_map_window(conn, stack_win->window);
                } else LOG("not remapping stackwin, height = %d, num_clients = %d\n",
                                stack_win->rect.height, num_clients);

                if (container->mode == MODE_TABBED) {
                        /* By setting num_clients to 1 we force that the stack window will be only one line
                         * high. The rest of the code is useful in both cases. */
                        LOG("tabbed mode, setting num_clients = 1\n");
                        if (stack_lines > 1)
                                stack_lines = 1;
                }

                if (container->stack_limit == STACK_LIMIT_COLS) {
                        stack_lines = ceil((float)num_clients / container->stack_limit_value);
                } else if (container->stack_limit == STACK_LIMIT_ROWS) {
                        stack_lines = min(num_clients, container->stack_limit_value);
                }

                /* Check if we need to reconfigure our stack title window */
                if (update_if_necessary(&(stack_win->rect.x), container->x) |
                    update_if_necessary(&(stack_win->rect.y), container->y) |
                    update_if_necessary(&(stack_win->rect.width), container->width) |
                    update_if_necessary(&(stack_win->rect.height), decoration_height * stack_lines)) {

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

                        /* Raise the stack window, but keep it below the first floating client
                         * and below the fullscreen client (if any) */
                        Client *first_floating = TAILQ_FIRST(&(container->workspace->floating_clients));
                        if (first_floating != TAILQ_END(&(container->workspace->floating_clients))) {
                                mask |= XCB_CONFIG_WINDOW_SIBLING;
                                values[4] = first_floating->frame;
                        } else if (container->workspace->fullscreen_client != NULL) {
                                mask |= XCB_CONFIG_WINDOW_SIBLING;
                                values[4] = container->workspace->fullscreen_client->frame;
                        }

                        xcb_configure_window(conn, stack_win->window, mask, values);
                }

                /* Prepare the pixmap for usage */
                cached_pixmap_prepare(conn, &(stack_win->pixmap));

                int current_row = 0, current_col = 0;
                int wrap = 0;

                if (container->stack_limit == STACK_LIMIT_COLS) {
                        /* wrap stores the number of rows after which we will
                         * wrap to a new column. */
                        wrap = ceil((float)num_clients / container->stack_limit_value);
                } else if (container->stack_limit == STACK_LIMIT_ROWS) {
                        /* When limiting rows, the wrap variable serves a
                         * slightly different purpose: it holds the number of
                         * pixels which each client will get. This is constant
                         * during the following loop, so it saves us some
                         * divisions and ceil()ing. */
                        wrap = (stack_win->rect.width / ceil((float)num_clients / container->stack_limit_value));
                }

                /* Render the decorations of all clients */
                CIRCLEQ_FOREACH(client, &(container->clients), clients) {
                        /* If the client is in fullscreen mode, it does not get reconfigured */
                        if (container->workspace->fullscreen_client == client) {
                                current_client++;
                                continue;
                        }

                        /* Check if we changed client->x or client->y by updating it.
                         * Note the bitwise OR instead of logical OR to force evaluation of all statements */
                        if (client->force_reconfigure |
                            update_if_necessary(&(client->rect.x), container->x) |
                            update_if_necessary(&(client->rect.y), container->y + (decoration_height * stack_lines)) |
                            update_if_necessary(&(client->rect.width), container->width) |
                            update_if_necessary(&(client->rect.height), container->height - (decoration_height * stack_lines)))
                                resize_client(conn, client);

                        client->force_reconfigure = false;

                        int offset_x = 0;
                        int offset_y = 0;
                        if (container->mode == MODE_STACK) {
                                if (container->stack_limit == STACK_LIMIT_COLS) {
                                        offset_x = current_col * (stack_win->rect.width / container->stack_limit_value);
                                        offset_y = current_row * decoration_height;
                                        current_row++;
                                        if ((current_row % wrap) == 0) {
                                                current_col++;
                                                current_row = 0;
                                        }
                                } else if (container->stack_limit == STACK_LIMIT_ROWS) {
                                        offset_x = current_col * wrap;
                                        offset_y = current_row * decoration_height;
                                        current_row++;
                                        if ((current_row % container->stack_limit_value) == 0) {
                                                current_col++;
                                                current_row = 0;
                                        }
                                } else {
                                        offset_y = current_client * decoration_height;
                                }
                                current_client++;
                        } else if (container->mode == MODE_TABBED)
                                offset_x = current_client++ * size_each;
                        decorate_window(conn, client, stack_win->pixmap.id, stack_win->pixmap.gc,
                                        offset_x, offset_y);
                }

                /* Check if we need to fill one column because of an uneven
                 * amount of windows */
                if (container->mode == MODE_STACK) {
                        if (container->stack_limit == STACK_LIMIT_COLS && (current_col % 2) != 0) {
                                xcb_change_gc_single(conn, stack_win->pixmap.gc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#000000"));

                                int offset_x = current_col * (stack_win->rect.width / container->stack_limit_value);
                                int offset_y = current_row * decoration_height;
                                xcb_rectangle_t rect = {offset_x, offset_y,
                                                        offset_x + container->width,
                                                        offset_y + decoration_height };
                                xcb_poly_fill_rectangle(conn, stack_win->pixmap.id, stack_win->pixmap.gc, 1, &rect);
                        } else if (container->stack_limit == STACK_LIMIT_ROWS && (current_row % 2) != 0) {
                                xcb_change_gc_single(conn, stack_win->pixmap.gc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#000000"));

                                int offset_x = current_col * wrap;
                                int offset_y = current_row * decoration_height;
                                xcb_rectangle_t rect = {offset_x, offset_y,
                                                        offset_x + container->width,
                                                        offset_y + decoration_height };
                                xcb_poly_fill_rectangle(conn, stack_win->pixmap.id, stack_win->pixmap.gc, 1, &rect);
                        }
                }

                xcb_copy_area(conn, stack_win->pixmap.id, stack_win->window, stack_win->pixmap.gc,
                              0, 0, 0, 0, stack_win->rect.width, stack_win->rect.height);
        }
}

static void render_bars(xcb_connection_t *conn, Workspace *r_ws, int width, int *height) {
        Client *client;
        SLIST_FOREACH(client, &(r_ws->screen->dock_clients), dock_clients) {
                LOG("client is at %d, should be at %d\n", client->rect.y, *height);
                if (client->force_reconfigure |
                    update_if_necessary(&(client->rect.x), r_ws->rect.x) |
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
        i3Font *font = load_font(conn, config.font);
        i3Screen *screen = r_ws->screen;
        enum { SET_NORMAL = 0, SET_FOCUSED = 1 };

        /* Fill the whole bar in black */
        xcb_change_gc_single(conn, screen->bargc, XCB_GC_FOREGROUND, get_colorpixel(conn, "#000000"));
        xcb_rectangle_t rect = {0, 0, width, height};
        xcb_poly_fill_rectangle(conn, screen->bar, screen->bargc, 1, &rect);

        /* Set font */
        xcb_change_gc_single(conn, screen->bargc, XCB_GC_FONT, font->id);

        int drawn = 0;
        Workspace *ws;
        TAILQ_FOREACH(ws, workspaces, workspaces) {
                if (ws->screen != screen)
                        continue;

                struct Colortriple *color;

                if (screen->current_workspace == ws)
                        color = &(config.bar.focused);
                else if (ws->urgent)
                        color = &(config.bar.urgent);
                else color = &(config.bar.unfocused);

                /* Draw the outer rect */
                xcb_draw_rect(conn, screen->bar, screen->bargc, color->border,
                              drawn,              /* x */
                              1,                  /* y */
                              ws->text_width + 5 + 5, /* width = text width + 5 px left + 5px right */
                              height - 2          /* height = max. height - 1 px upper and 1 px bottom border */);

                /* Draw the background of this rect */
                xcb_draw_rect(conn, screen->bar, screen->bargc, color->background,
                              drawn + 1,
                              2,
                              ws->text_width + 4 + 4,
                              height - 4);

                xcb_change_gc_single(conn, screen->bargc, XCB_GC_FOREGROUND, color->text);
                xcb_change_gc_single(conn, screen->bargc, XCB_GC_BACKGROUND, color->background);
                xcb_image_text_16(conn, ws->name_len, screen->bar, screen->bargc, drawn + 5 /* X */,
                                  font->height + 1 /* Y = baseline of font */,
                                  (xcb_char2b_t*)ws->name);
                drawn += ws->text_width + 12;
        }
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

        FOR_TABLE(workspace) {
                if (workspace->table[cols][rows] == NULL)
                        continue;

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
}

/*
 * Renders the given workspace on the given screen
 *
 */
void render_workspace(xcb_connection_t *conn, i3Screen *screen, Workspace *r_ws) {
        i3Font *font = load_font(conn, config.font);
        int width = r_ws->rect.width;
        int height = r_ws->rect.height;

        /* Reserve space for dock clients */
        Client *client;
        SLIST_FOREACH(client, &(screen->dock_clients), dock_clients)
                height -= client->desired_height;

        /* Space for the internal bar */
        height -= (font->height + 6);

        int xoffset[r_ws->rows];
        int yoffset[r_ws->cols];
        /* Initialize offsets */
        for (int cols = 0; cols < r_ws->cols; cols++)
                yoffset[cols] = r_ws->rect.y;
        for (int rows = 0; rows < r_ws->rows; rows++)
                xoffset[rows] = r_ws->rect.x;

        ignore_enter_notify_forall(conn, r_ws, true);

        /* Go through the whole table and render what’s necessary */
        FOR_TABLE(r_ws) {
                Container *container = r_ws->table[cols][rows];
                if (container == NULL)
                        continue;
                int single_width = -1, single_height = -1;
                /* Update position of the container */
                container->row = rows;
                container->col = cols;
                container->x = xoffset[rows];
                container->y = yoffset[cols];
                container->width = 0;

                for (int c = 0; c < container->colspan; c++) {
                        if (r_ws->width_factor[cols+c] == 0)
                                container->width += (width / r_ws->cols);
                        else container->width += get_unoccupied_x(r_ws) * r_ws->width_factor[cols+c];

                        if (single_width == -1)
                                single_width = container->width;
                }

                LOG("height is %d\n", height);

                container->height = 0;

                for (int c = 0; c < container->rowspan; c++) {
                        if (r_ws->height_factor[rows+c] == 0)
                                container->height += (height / r_ws->rows);
                        else container->height += get_unoccupied_y(r_ws) * r_ws->height_factor[rows+c];

                        if (single_height == -1)
                                single_height = container->height;
                }

                /* Render the container if it is not empty */
                render_container(conn, container);

                xoffset[rows] += single_width;
                yoffset[cols] += single_height;
        }

        ignore_enter_notify_forall(conn, r_ws, false);

        render_bars(conn, r_ws, width, &height);
        render_internal_bar(conn, r_ws, width, font->height + 6);
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

        if (virtual_screens == NULL)
                return;

        TAILQ_FOREACH(screen, virtual_screens, screens)
                if (screen->current_workspace != NULL)
                        render_workspace(conn, screen, screen->current_workspace);

        xcb_flush(conn);
}
