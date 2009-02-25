/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xinerama.h>

#include "queue.h"
#include "i3.h"
#include "data.h"
#include "table.h"
#include "util.h"
#include "xinerama.h"

/* This TAILQ of i3Screens stores the virtual screens, used for handling overlapping screens
 * (xrandr --same-as) */
struct screens_head virtual_screens = TAILQ_HEAD_INITIALIZER(virtual_screens);

/*
 * Looks in virtual_screens for the i3Screen whose start coordinates are x, y
 *
 */
i3Screen *get_screen_at(int x, int y) {
        i3Screen *screen;
        TAILQ_FOREACH(screen, &virtual_screens, screens)
                if (screen->rect.x == x && screen->rect.y == y)
                        return screen;

        return NULL;
}

/*
 * Looks in virtual_screens for the i3Screen which contains coordinates x, y
 *
 */
i3Screen *get_screen_containing(int x, int y) {
        i3Screen *screen;
        TAILQ_FOREACH(screen, &virtual_screens, screens)
                if (x > screen->rect.x && x < (screen->rect.x + screen->rect.width) &&
                    y > screen->rect.y && y < (screen->rect.y + screen->rect.height))
                        return screen;

        return NULL;
}

/*
 * Fills virtual_screens with exactly one screen with width/height of the whole X server.
 *
 */
static void disable_xinerama(xcb_connection_t *connection) {
        xcb_screen_t *root_screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;

        i3Screen *s = calloc(sizeof(i3Screen), 1);

        s->rect.x = 0;
        s->rect.y = 0;
        s->rect.width = root_screen->width_in_pixels;
        s->rect.height = root_screen->height_in_pixels;

        TAILQ_INSERT_TAIL(&virtual_screens, s, screens);
}

/*
 * We have just established a connection to the X server and need the initial Xinerama
 * information to setup workspaces for each screen.
 *
 */
void initialize_xinerama(xcb_connection_t *conn) {
        xcb_xinerama_query_screens_reply_t *reply;
        xcb_xinerama_screen_info_t *screen_info;

        if (!xcb_get_extension_data(conn, &xcb_xinerama_id)->present) {
                printf("Xinerama extension not found, disabling.\n");
                disable_xinerama(conn);
                return;
        }

        if (!xcb_xinerama_is_active_reply(conn, xcb_xinerama_is_active(conn), NULL)->state) {
                printf("Xinerama is not active (in your X-Server), disabling.\n");
                disable_xinerama(conn);
                return;
        }

        reply = xcb_xinerama_query_screens_reply(conn, xcb_xinerama_query_screens_unchecked(conn), NULL);
        if (!reply) {
                printf("Couldn’t get active Xinerama screens\n");
                return;
        }
        screen_info = xcb_xinerama_query_screens_screen_info(reply);
        num_screens = xcb_xinerama_query_screens_screen_info_length(reply);

        /* Just go through each workspace and associate as many screens as we can. */
        for (int screen = 0; screen < num_screens; screen++) {
                i3Screen *s = get_screen_at(screen_info[screen].x_org, screen_info[screen].y_org);
                if (s!= NULL) {
                        /* This screen already exists. We use the littlest screen so that the user
                           can always see the complete workspace */
                        s->rect.width = min(s->rect.width, screen_info[screen].width);
                        s->rect.height = min(s->rect.height, screen_info[screen].height);
                } else {
                        s = calloc(sizeof(i3Screen), 1);
                        s->rect.x = screen_info[screen].x_org;
                        s->rect.y = screen_info[screen].y_org;
                        s->rect.width = screen_info[screen].width;
                        s->rect.height = screen_info[screen].height;
                        TAILQ_INSERT_TAIL(&virtual_screens, s, screens);
                }

                printf("found Xinerama screen: %d x %d at %d x %d\n",
                                screen_info[screen].width, screen_info[screen].height,
                                screen_info[screen].x_org, screen_info[screen].y_org);
        }

        i3Screen *s;
        num_screens = 0;
        TAILQ_FOREACH(s, &virtual_screens, screens) {
                s->num = num_screens;
                s->current_workspace = num_screens;
                workspaces[num_screens].screen = s;
                memcpy(&(workspaces[num_screens++].rect), &(s->rect), sizeof(Rect));
                printf("that is virtual screen at %d x %d with %d x %d\n",
                                s->rect.x, s->rect.y, s->rect.width, s->rect.height);
        }

        free(reply);
}
