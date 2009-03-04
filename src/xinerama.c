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
#include <stdbool.h>

#include <xcb/xcb.h>
#include <xcb/xinerama.h>

#include "queue.h"
#include "i3.h"
#include "data.h"
#include "table.h"
#include "util.h"
#include "xinerama.h"
#include "layout.h"
#include "xcb.h"
#include "config.h"

/* This TAILQ of i3Screens stores the virtual screens, used for handling overlapping screens
 * (xrandr --same-as) */
struct screens_head *virtual_screens;

static bool xinerama_enabled = true;

/*
 * Looks in virtual_screens for the i3Screen whose start coordinates are x, y
 *
 */
i3Screen *get_screen_at(int x, int y, struct screens_head *screenlist) {
        i3Screen *screen;
        TAILQ_FOREACH(screen, screenlist, screens)
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
        TAILQ_FOREACH(screen, virtual_screens, screens) {
                printf("comparing x=%d y=%d with x=%d and y=%d width %d height %d\n",
                                x, y, screen->rect.x, screen->rect.y, screen->rect.width, screen->rect.height);
                if (x >= screen->rect.x && x < (screen->rect.x + screen->rect.width) &&
                    y >= screen->rect.y && y < (screen->rect.y + screen->rect.height))
                        return screen;
        }

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

        TAILQ_INSERT_TAIL(virtual_screens, s, screens);

        xinerama_enabled = false;
}

/*
 * Gets the Xinerama screens and converts them to virtual i3Screens (only one screen for two
 * Xinerama screen which are configured in clone mode) in the given screenlist
 *
 */
static void query_screens(xcb_connection_t *connection, struct screens_head *screenlist) {
        xcb_xinerama_query_screens_reply_t *reply;
        xcb_xinerama_screen_info_t *screen_info;

        reply = xcb_xinerama_query_screens_reply(connection, xcb_xinerama_query_screens_unchecked(connection), NULL);
        if (!reply) {
                printf("Couldn't get Xinerama screens\n");
                return;
        }
        screen_info = xcb_xinerama_query_screens_screen_info(reply);
        int screens = xcb_xinerama_query_screens_screen_info_length(reply);
        num_screens = 0;

        for (int screen = 0; screen < screens; screen++) {
                i3Screen *s = get_screen_at(screen_info[screen].x_org, screen_info[screen].y_org, screenlist);
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
                        /* We always treat the screen at 0x0 as the primary screen */
                        if (s->rect.x == 0 && s->rect.y == 0)
                                TAILQ_INSERT_HEAD(screenlist, s, screens);
                        else TAILQ_INSERT_TAIL(screenlist, s, screens);
                        num_screens++;
                }

                printf("found Xinerama screen: %d x %d at %d x %d\n",
                                screen_info[screen].width, screen_info[screen].height,
                                screen_info[screen].x_org, screen_info[screen].y_org);
        }

        free(reply);
}

static void initialize_screen(xcb_connection_t *connection, i3Screen *screen, Workspace *workspace) {
        i3Font *font = load_font(connection, config.font);

        workspace->screen = screen;
        screen->current_workspace = workspace->num;

        /* Create a bar for each screen */
        Rect bar_rect = {screen->rect.x,
                         screen->rect.height - (font->height + 6),
                         screen->rect.x + screen->rect.width,
                         font->height + 6};
        uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
        uint32_t values[] = {1, XCB_EVENT_MASK_EXPOSURE};
        screen->bar = create_window(connection, bar_rect, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_CURSOR_LEFT_PTR, mask, values);
        screen->bargc = xcb_generate_id(connection);
        xcb_create_gc(connection, screen->bargc, screen->bar, 0, 0);

        /* Copy dimensions */
        memcpy(&(workspace->rect), &(screen->rect), sizeof(Rect));
        printf("that is virtual screen at %d x %d with %d x %d\n",
                        screen->rect.x, screen->rect.y, screen->rect.width, screen->rect.height);
}

/*
 * We have just established a connection to the X server and need the initial Xinerama
 * information to setup workspaces for each screen.
 *
 */
void initialize_xinerama(xcb_connection_t *connection) {
        virtual_screens = scalloc(sizeof(struct screens_head));
        TAILQ_INIT(virtual_screens);

        if (!xcb_get_extension_data(connection, &xcb_xinerama_id)->present) {
                printf("Xinerama extension not found, disabling.\n");
                disable_xinerama(connection);
                return;
        }

        if (!xcb_xinerama_is_active_reply(connection, xcb_xinerama_is_active(connection), NULL)->state) {
                printf("Xinerama is not active (in your X-Server), disabling.\n");
                disable_xinerama(connection);
                return;
        }

        query_screens(connection, virtual_screens);

        i3Screen *s;
        num_screens = 0;
        /* Just go through each workspace and associate as many screens as we can. */
        TAILQ_FOREACH(s, virtual_screens, screens) {
                s->num = num_screens;
                initialize_screen(connection, s, &(workspaces[num_screens]));
                num_screens++;
        }
}

/*
 * This is called when the rootwindow receives a configure_notify event and therefore the
 * number/position of the Xinerama screens could have changed.
 *
 */
void xinerama_requery_screens(xcb_connection_t *connection) {
        /* POSSIBLE PROBLEM: Is the order of the Xinerama screens always constant? That is, can
           it change when I move the --right-of video projector to --left-of? */

        if (!xinerama_enabled) {
                printf("Xinerama is disabled\n");
                return;
        }

        /* We use a separate copy to diff with the previous set of screens */
        struct screens_head *new_screens = scalloc(sizeof(struct screens_head));
        TAILQ_INIT(new_screens);

        query_screens(connection, new_screens);

        i3Screen *first = TAILQ_FIRST(new_screens),
                 *screen;
        int screen_count = 0;
        TAILQ_FOREACH(screen, new_screens, screens) {
                screen->num = screen_count;
                screen->current_workspace = -1;
                for (int c = 0; c < 10; c++)
                        if ((workspaces[c].screen != NULL) &&
                            (workspaces[c].screen->num == screen_count)) {
                                printf("Found a matching screen\n");
                                /* Try to use the same workspace, if it’s available */
                                if (workspaces[c].screen->current_workspace)
                                        screen->current_workspace = workspaces[c].screen->current_workspace;

                                if (screen->current_workspace == -1)
                                        screen->current_workspace = c;

                                /* Re-use the old bar window */
                                screen->bar = workspaces[c].screen->bar;
                                screen->bargc = workspaces[c].screen->bargc;

                                /* Update the dimensions */
                                memcpy(&(workspaces[c].rect), &(screen->rect), sizeof(Rect));
                                workspaces[c].screen = screen;
                        }
                if (screen->current_workspace == -1) {
                        /* Create a new workspace for this screen, it’s new */
                        for (int c = 0; c < 10; c++)
                                if (workspaces[c].screen == NULL) {
                                        printf("fix: initializing new workspace, setting num to %d\n", c);
                                        initialize_screen(connection, screen, &(workspaces[c]));
                                        break;
                                }
                }
                screen_count++;
        }

        /* Check for workspaces which are out of bounds */
        for (int c = 0; c < 10; c++)
                if ((workspaces[c].screen != NULL) &&
                    (workspaces[c].screen->num >= num_screens)) {
                        printf("Closing bar window\n");
                        xcb_destroy_window(connection, workspaces[c].screen->bar);

                        printf("Workspace %d's screen out of bounds, assigning to first screen\n", c+1);
                        workspaces[c].screen = first;
                        memcpy(&(workspaces[c].rect), &(first->rect), sizeof(Rect));
                }

        /* Free the old list */
        TAILQ_FOREACH(screen, virtual_screens, screens) {
                TAILQ_REMOVE(virtual_screens, screen, screens);
                free(screen);
        }
        free(virtual_screens);

        virtual_screens = new_screens;

        printf("Current workspace is now: %d\n", first->current_workspace);

        render_layout(connection);
}
