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
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <xcb/xcb.h>

#include "font.h"
#include "i3.h"
#include "xcb.h"
#include "table.h"
#include "util.h"
#include "xinerama.h"

/*
 * For resizing containers (= cells), we need to know the space which is unoccupied by "default"
 * windows. The resized containers will be rendered relatively to this space, meaning that newly
 * created columns/rows after a container was resized will start with their normal size.
 *
 */
Rect get_unoccupied_space(Workspace *workspace) {
        printf("getting unoccupied space\n");
        float default_factor_w = ((float)workspace->rect.width / (float)workspace->cols) / (float)workspace->rect.width;
        float default_factor_h = (workspace->rect.height / workspace->rows) / workspace->rect.height;
        Rect result = {0, 0, workspace->rect.width, workspace->rect.height};

        printf("default factor is %f and %f\n", default_factor_w, default_factor_h);
        printf("start w = %d, h = %d\n", result.width, result.height);
        /* TODO: colspan/rowspan*/

        for (int cols = 0; cols < workspace->cols; cols++)
                for (int rows = 0; rows < workspace->rows; rows++) {
                        printf("oh hai. wf[%d][%d] = %f\n", cols, rows, workspace->table[cols][rows]->width_factor);
                        if (workspace->table[cols][rows]->width_factor == 0)
                                result.width -= workspace->rect.width * default_factor_w;
                        if (workspace->table[cols][rows]->height_factor == 0)
                                result.height -= workspace->rect.height * default_factor_h;
                }

        /* If every container is using the default factor, we have the whole space available */
        if (result.width == 0)
                result.width = workspace->rect.width;

        if (result.height == 0)
                result.height = workspace->rect.height;

        printf("unoccupied x = %d, unoccupied y = %d\n", result.width, result.height);

        return result;
}

/*
 * (Re-)draws window decorations for a given Client
 *
 */
void decorate_window(xcb_connection_t *conn, Client *client) {
        uint32_t mask = 0;
        uint32_t values[3];
        i3Font *font = load_font(conn, pattern);
        uint32_t background_color,
                 text_color,
                 border_color;

        if (client->container->currently_focused == client) {
                background_color = get_colorpixel(conn, client->frame, "#285577");
                text_color = get_colorpixel(conn, client->frame, "#ffffff");
                border_color = get_colorpixel(conn, client->frame, "#4c7899");
        } else {
                background_color = get_colorpixel(conn, client->frame, "#222222");
                text_color = get_colorpixel(conn, client->frame, "#888888");
                border_color = get_colorpixel(conn, client->frame, "#333333");
        }

        /* Our plan is the following:
           - Draw a rect around the whole client in background_color
           - Draw two lines in a lighter color
           - Draw the window’s title

           Note that xcb_image_text apparently adds 1xp border around the font? Can anyone confirm this?
         */

        /* Draw a green rectangle around the window */
        mask = XCB_GC_FOREGROUND;
        values[0] = background_color;
        xcb_change_gc(conn, client->titlegc, mask, values);

        xcb_rectangle_t rect = {0, 0, client->rect.width, client->rect.height};
        xcb_poly_fill_rectangle(conn, client->frame, client->titlegc, 1, &rect);

        /* Draw the lines */
        /* TODO: this needs to be more beautiful somewhen. maybe stdarg + change_gc(gc, ...) ? */
#define DRAW_LINE(colorpixel, x, y, to_x, to_y) { \
                uint32_t draw_values[1]; \
                draw_values[0] = colorpixel; \
                xcb_change_gc(conn, client->titlegc, XCB_GC_FOREGROUND, draw_values); \
                xcb_point_t points[] = {{x, y}, {to_x, to_y}}; \
                xcb_poly_line(conn, XCB_COORD_MODE_ORIGIN, client->frame, client->titlegc, 2, points); \
        }

        DRAW_LINE(border_color, 2, 0, client->rect.width, 0);
        DRAW_LINE(border_color, 2, font->height + 3, 2 + client->rect.width, font->height + 3);

        /* Draw the font */
        mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;

        values[0] = text_color;
        values[1] = background_color;
        values[2] = font->id;

        xcb_change_gc(conn, client->titlegc, mask, values);

        /* TODO: utf8? */
        char *label;
        asprintf(&label, "(%08x) %.*s", client->frame, client->name_len, client->name);
        xcb_void_cookie_t text_cookie = xcb_image_text_8_checked(conn, strlen(label), client->frame,
                                        client->titlegc, 3 /* X */, font->height /* Y = baseline of font */, label);
        check_error(conn, text_cookie, "Could not draw client's title");
        free(label);
}

static void render_container(xcb_connection_t *connection, Container *container) {
        Client *client;
        i3Font *font = load_font(connection, pattern);

        if (container->mode == MODE_DEFAULT) {
                int num_clients = 0;
                CIRCLEQ_FOREACH(client, &(container->clients), clients)
                        num_clients++;
                printf("got %d clients in this default container.\n", num_clients);

                int current_client = 0;
                CIRCLEQ_FOREACH(client, &(container->clients), clients) {
                        /* TODO: at the moment, every column/row is screen / num_cols. This
                         * needs to be changed to "percentage of the screen" by
                         * default and adjustable by the user if necessary.
                         */

                        /* Check if we changed client->x or client->y by updating it…
                         * Note the bitwise OR instead of logical OR to force evaluation of both statements */
                        if (client->force_reconfigure |
                            (client->rect.x != (client->rect.x = container->x)) |
                            (client->rect.y != (client->rect.y = container->y +
                                          (container->height / num_clients) * current_client))) {
                                printf("frame needs to be pushed to %dx%d\n", client->rect.x, client->rect.y);
                                /* Note: We can use a pointer to client->x like an array of uint32_ts
                                   because it is followed by client->y by definition */
                                xcb_configure_window(connection, client->frame,
                                                XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, &(client->rect.x));
                        }

                        /* TODO: vertical default layout */
                        if (client->force_reconfigure |
                            (client->rect.width != (client->rect.width = container->width)) |
                            (client->rect.height != (client->rect.height = container->height / num_clients))) {
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
                                uint32_t values[4] = {2,                                                   /* x */
                                                      font->height + 2 + 2,                                /* y */
                                                      client->rect.width - (2 + 2),                        /* width */
                                                      client->rect.height - ((font->height + 2 + 2) + 2)}; /* height */

                                printf("child will be at %dx%d with size %dx%d\n",
                                                values[0], values[1], values[2], values[3]);

                                xcb_configure_window(connection, client->child, mask, values);
                        }

                        if (client->force_reconfigure)
                                client->force_reconfigure = false;

                        current_client++;
                }
        } else {
                /* TODO: Implement stacking */
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
                int x = r_ws->rect.x;
                int y = r_ws->rect.y;

                printf("got %d rows and %d cols\n", r_ws->rows, r_ws->cols);
                printf("each of them therefore is %d px width and %d px height\n",
                                width / r_ws->cols, height / r_ws->rows);

                Rect space = get_unoccupied_space(r_ws);
                printf("got %d / %d unoc space\n", space.width, space.height);

                /* Go through the whole table and render what’s necessary */
                for (int cols = 0; cols < r_ws->cols; cols++)
                        for (int rows = 0; rows < r_ws->rows; rows++) {
                                Container *container = r_ws->table[cols][rows];
                                printf("container has %d colspan, %d rowspan\n",
                                                container->colspan, container->rowspan);
                                printf("container at %d, %d\n", x, y);
                                /* Update position of the container */
                                container->row = rows;
                                container->col = cols;
                                container->x = x;
                                container->y = y;
                                if (container->width_factor == 0)
                                        container->width = (width / r_ws->cols) * container->colspan;
                                else container->width = space.width * container->width_factor;
                                container->height = (height / r_ws->rows) * container->rowspan;

                                /* Render it */
                                render_container(connection, container);

                                x += container->width;
                        }
        }

        xcb_flush(connection);
}
