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

#include "config.h"
#include "font.h"
#include "i3.h"
#include "xcb.h"
#include "table.h"
#include "util.h"
#include "xinerama.h"

/*
 * Gets the unoccupied space (= space which is available for windows which were resized by the user)
 * for the given row. This is necessary to render both, customly resized windows and never touched
 * windows correctly, meaning that the aspect ratio will be maintained when opening new windows.
 *
 */
int get_unoccupied_x(Workspace *workspace, int row) {
        int unoccupied = workspace->rect.width;
        float default_factor = ((float)workspace->rect.width / workspace->cols) / workspace->rect.width;

        printf("get_unoccupied_x(), starting with %d, default_factor = %f\n", unoccupied, default_factor);

        for (int cols = 0; cols < workspace->cols;) {
                Container *con = workspace->table[cols][row];
                printf("width_factor[%d][%d] = %f, colspan = %d\n", cols, row, con->width_factor, con->colspan);
                if (con->width_factor == 0)
                        unoccupied -= workspace->rect.width * default_factor * con->colspan;
                cols += con->colspan;
        }

        assert(unoccupied != 0);

        printf("unoccupied space: %d\n", unoccupied);
        return unoccupied;
}

/* See get_unoccupied_x() */
int get_unoccupied_y(Workspace *workspace, int col) {
        int unoccupied = workspace->rect.height;
        float default_factor = ((float)workspace->rect.height / workspace->rows) / workspace->rect.height;

        printf("get_unoccupied_y(), starting with %d, default_factor = %f\n", unoccupied, default_factor);

        for (int rows = 0; rows < workspace->rows;) {
                Container *con = workspace->table[col][rows];
                printf("height_factor[%d][%d] = %f, rowspan %d\n", col, rows, con->height_factor, con->rowspan);
                if (con->height_factor == 0)
                        unoccupied -= workspace->rect.height * default_factor * con->rowspan;
                rows += con->rowspan;
        }

        assert(unoccupied != 0);

        printf("unoccupied space: %d\n", unoccupied);
        return unoccupied;
}

/*
 * (Re-)draws window decorations for a given Client onto the given drawable/graphic context.
 * When in stacking mode, the window decorations are drawn onto an own window.
 *
 */
void decorate_window(xcb_connection_t *conn, Client *client, xcb_drawable_t drawable, xcb_gcontext_t gc, int offset) {
        i3Font *font = load_font(conn, config.font);
        uint32_t background_color,
                 text_color,
                 border_color;

        /* Clients without a container (docks) won’t get decorated */
        if (client->container == NULL)
                return;

        if (client->container->currently_focused == client) {
                background_color = get_colorpixel(conn, client, client->frame, "#285577");
                text_color = get_colorpixel(conn, client, client->frame, "#ffffff");
                border_color = get_colorpixel(conn, client, client->frame, "#4c7899");
        } else {
                background_color = get_colorpixel(conn, client, client->frame, "#222222");
                text_color = get_colorpixel(conn, client, client->frame, "#888888");
                border_color = get_colorpixel(conn, client, client->frame, "#333333");
        }

        /* Our plan is the following:
           - Draw a rect around the whole client in background_color
           - Draw two lines in a lighter color
           - Draw the window’s title
         */

        /* Draw a green rectangle around the window */
        xcb_change_gc_single(conn, gc, XCB_GC_FOREGROUND, background_color);

        xcb_rectangle_t rect = {0, offset, client->rect.width, offset + client->rect.height};
        xcb_poly_fill_rectangle(conn, drawable, gc, 1, &rect);

        /* Draw the lines */
        xcb_draw_line(conn, drawable, gc, border_color, 2, offset, client->rect.width, offset);
        xcb_draw_line(conn, drawable, gc, border_color, 2, offset + font->height + 3,
                      2 + client->rect.width, offset + font->height + 3);

        /* Draw the font */
        uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
        uint32_t values[] = { text_color, background_color, font->id };
        xcb_change_gc(conn, gc, mask, values);

        /* TODO: utf8? */
        char *label;
        asprintf(&label, "(%08x) %.*s", client->frame, client->name_len, client->name);
        xcb_void_cookie_t text_cookie = xcb_image_text_8_checked(conn, strlen(label), drawable,
                                        gc, 3 /* X */, offset + font->height /* Y = baseline of font */, label);
        check_error(conn, text_cookie, "Could not draw client's title");
        free(label);
}

/*
 * Pushes the client’s x and y coordinates to X11
 *
 */
static void reposition_client(xcb_connection_t *connection, Client *client) {
        printf("frame needs to be pushed to %dx%d\n", client->rect.x, client->rect.y);
        /* Note: We can use a pointer to client->x like an array of uint32_ts
           because it is followed by client->y by definition */
        xcb_configure_window(connection, client->frame,
                        XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, &(client->rect.x));
}

/*
 * Pushes the client’s width/height to X11 and resizes the child window
 *
 */
static void resize_client(xcb_connection_t *connection, Client *client) {
        i3Font *font = load_font(connection, config.font);

        printf("resizing client to %d x %d\n", client->rect.width, client->rect.height);
        xcb_configure_window(connection, client->frame,
                        XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                        &(client->rect.width));

        /* Adjust the position of the child inside its frame.
         * The coordinates of the child are relative to its frame, we
         * add a border of 2 pixel to each value */
        uint32_t mask = XCB_CONFIG_WINDOW_X |
                        XCB_CONFIG_WINDOW_Y |
                        XCB_CONFIG_WINDOW_WIDTH |
                        XCB_CONFIG_WINDOW_HEIGHT;
        Rect rect;
        if (client->titlebar_position == TITLEBAR_OFF ||
            client->container->mode == MODE_STACK) {
                rect.x = 0;
                rect.y = 0;
                rect.width = client->rect.width;
                rect.height = client->rect.height;
        } else {
                rect.x = 2;
                rect.y = font->height + 2 + 2;
                rect.width = client->rect.width - (2 + 2);
                rect.height = client->rect.height - ((font->height + 2 + 2) + 2);
        }

        printf("child will be at %dx%d with size %dx%d\n", rect.x, rect.y, rect.width, rect.height);

        xcb_configure_window(connection, client->child, mask, &(rect.x));
}

/*
 * Renders the given container. Is called by render_layout() or individually (for example
 * when focus changes in a stacking container)
 *
 */
void render_container(xcb_connection_t *connection, Container *container) {
        Client *client;
        int num_clients = 0, current_client = 0;

        if (container->currently_focused == NULL)
                return;

        CIRCLEQ_FOREACH(client, &(container->clients), clients)
                num_clients++;

        if (container->mode == MODE_DEFAULT) {
                printf("got %d clients in this default container.\n", num_clients);
                CIRCLEQ_FOREACH(client, &(container->clients), clients) {
                        /* Check if we changed client->x or client->y by updating it.
                         * Note the bitwise OR instead of logical OR to force evaluation of both statements */
                        if (client->force_reconfigure |
                            (client->rect.x != (client->rect.x = container->x)) |
                            (client->rect.y != (client->rect.y = container->y +
                                          (container->height / num_clients) * current_client))) {
                                reposition_client(connection, client);
                        }

                        /* TODO: vertical default layout */
                        if (client->force_reconfigure |
                            (client->rect.width != (client->rect.width = container->width)) |
                            (client->rect.height != (client->rect.height = container->height / num_clients))) {
                                resize_client(connection, client);
                        }

                        if (client->force_reconfigure)
                                client->force_reconfigure = false;

                        current_client++;
                }
        } else {
                i3Font *font = load_font(connection, config.font);
                int decoration_height = (font->height + 2 + 2);
                struct Stack_Window *stack_win = &(container->stack_win);

                /* Check if we need to remap our stack title window, it gets unmapped when the container
                   is empty in src/handlers.c:unmap_notify() */
                if (stack_win->height == 0)
                        xcb_map_window(connection, stack_win->window);

                /* Check if we need to reconfigure our stack title window */
                if ((stack_win->width != (stack_win->width = container->width)) |
                    (stack_win->height != (stack_win->height = decoration_height * num_clients)))
                        xcb_configure_window(connection, stack_win->window,
                                XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, &(stack_win->width));

                /* All clients are repositioned */
                CIRCLEQ_FOREACH(client, &(container->clients), clients) {
                        /* Check if we changed client->x or client->y by updating it.
                         * Note the bitwise OR instead of logical OR to force evaluation of both statements */
                        if (client->force_reconfigure |
                            (client->rect.x != (client->rect.x = container->x)) |
                            (client->rect.y != (client->rect.y = container->y + (decoration_height * num_clients))))
                                reposition_client(connection, client);

                        if (client->force_reconfigure |
                            (client->rect.width != (client->rect.width = container->width)) |
                            (client->rect.height !=
                             (client->rect.height = container->height - (decoration_height * num_clients))))
                                resize_client(connection, client);

                        client->force_reconfigure = false;

                        decorate_window(connection, client, stack_win->window, stack_win->gc,
                                        current_client * decoration_height);
                        current_client++;
                }
        }
}

static void render_bars(xcb_connection_t *connection, Workspace *r_ws, int width, int height) {
        Client *client;
        SLIST_FOREACH(client, &(r_ws->dock_clients), dock_clients) {
                if (client->force_reconfigure |
                    (client->rect.x != (client->rect.x = 0)) |
                    (client->rect.y != (client->rect.y = height)))
                        reposition_client(connection, client);

                if (client->force_reconfigure |
                    (client->rect.width != (client->rect.width = width)) |
                    (client->rect.height != (client->rect.height = client->desired_height)))
                        resize_client(connection, client);

                client->force_reconfigure = false;
                height += client->desired_height;
        }
}

void render_layout(xcb_connection_t *connection) {
        i3Screen *screen;

        TAILQ_FOREACH(screen, &virtual_screens, screens) {
                /* r_ws (rendering workspace) is just a shortcut to the Workspace being currently rendered */
                Workspace *r_ws = &(workspaces[screen->current_workspace]);

                printf("Rendering screen %d\n", screen->num);
                if (r_ws->fullscreen_client != NULL)
                        /* This is easy: A client has entered fullscreen mode, so we don’t render at all */
                        continue;

                int width = r_ws->rect.width;
                int height = r_ws->rect.height;

                /* Reserve space for dock clients */
                Client *client;
                SLIST_FOREACH(client, &(r_ws->dock_clients), dock_clients)
                        height -= client->desired_height;

                printf("got %d rows and %d cols\n", r_ws->rows, r_ws->cols);

                int xoffset[r_ws->rows];
                int yoffset[r_ws->cols];
                /* Initialize offsets */
                for (int cols = 0; cols < r_ws->cols; cols++)
                        yoffset[cols] = r_ws->rect.y;
                for (int rows = 0; rows < r_ws->rows; rows++)
                        xoffset[rows] = r_ws->rect.x;

                /* Go through the whole table and render what’s necessary */
                for (int cols = 0; cols < r_ws->cols; cols++)
                        for (int rows = 0; rows < r_ws->rows; rows++) {
                                Container *container = r_ws->table[cols][rows];
                                printf("\n========\ncontainer has %d colspan, %d rowspan\n",
                                                container->colspan, container->rowspan);
                                printf("container at %d, %d\n", xoffset[rows], yoffset[cols]);
                                /* Update position of the container */
                                container->row = rows;
                                container->col = cols;
                                container->x = xoffset[rows];
                                container->y = yoffset[cols];

                                if (container->width_factor == 0)
                                        container->width = (width / r_ws->cols);
                                else container->width = get_unoccupied_x(r_ws, rows) * container->width_factor;
                                container->width *= container->colspan;

                                if (container->height_factor == 0)
                                        container->height = (height / r_ws->rows);
                                else container->height = get_unoccupied_y(r_ws, cols) * container->height_factor;
                                container->height *= container->rowspan;

                                /* Render the container if it is not empty */
                                render_container(connection, container);

                                xoffset[rows] += container->width;
                                yoffset[cols] += container->height;
                                printf("==========\n");
                        }

                render_bars(connection, r_ws, width, height);
        }

        xcb_flush(connection);
}
