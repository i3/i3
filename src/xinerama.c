/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * This is LEGACY code (we support RandR, which can do much more than
 * Xinerama), but necessary for the poor users of the nVidia binary
 * driver which does not support RandR in 2011 *sigh*.
 *
 */
#include "all.h"

#include <xcb/xinerama.h>

static int num_screens;

/*
 * Looks in outputs for the Output whose start coordinates are x, y
 *
 */
static Output *get_screen_at(unsigned int x, unsigned int y) {
    Output *output;
    TAILQ_FOREACH (output, &outputs, outputs) {
        if (output->rect.x == x && output->rect.y == y) {
            return output;
        }
    }

    return NULL;
}

/*
 * Gets the Xinerama screens and converts them to virtual Outputs (only one screen for two
 * Xinerama screen which are configured in clone mode) in the given screenlist
 *
 */
static void query_screens(xcb_connection_t *conn) {
    xcb_xinerama_query_screens_reply_t *reply;
    xcb_xinerama_screen_info_t *screen_info;

    reply = xcb_xinerama_query_screens_reply(conn, xcb_xinerama_query_screens_unchecked(conn), NULL);
    if (!reply) {
        ELOG("Couldn't get Xinerama screens\n");
        return;
    }
    screen_info = xcb_xinerama_query_screens_screen_info(reply);
    int screens = xcb_xinerama_query_screens_screen_info_length(reply);

    for (int screen = 0; screen < screens; screen++) {
        Output *s = get_screen_at(screen_info[screen].x_org, screen_info[screen].y_org);
        if (s != NULL) {
            DLOG("Re-used old Xinerama screen %p\n", s);
            /* This screen already exists. We use the littlest screen so that the user
               can always see the complete workspace */
            s->rect.width = min(s->rect.width, screen_info[screen].width);
            s->rect.height = min(s->rect.height, screen_info[screen].height);
        } else {
            s = scalloc(1, sizeof(Output));
            struct output_name *output_name = scalloc(1, sizeof(struct output_name));
            sasprintf(&output_name->name, "xinerama-%d", num_screens);
            SLIST_INIT(&s->names_head);
            SLIST_INSERT_HEAD(&s->names_head, output_name, names);
            DLOG("Created new Xinerama screen %s (%p)\n", output_primary_name(s), s);
            s->active = true;
            s->rect.x = screen_info[screen].x_org;
            s->rect.y = screen_info[screen].y_org;
            s->rect.width = screen_info[screen].width;
            s->rect.height = screen_info[screen].height;
            /* We always treat the screen at 0x0 as the primary screen */
            if (s->rect.x == 0 && s->rect.y == 0) {
                TAILQ_INSERT_HEAD(&outputs, s, outputs);
            } else {
                TAILQ_INSERT_TAIL(&outputs, s, outputs);
            }
            output_init_con(s);
            init_ws_for_output(s);
            num_screens++;
        }

        DLOG("found Xinerama screen: %d x %d at %d x %d\n",
             screen_info[screen].width, screen_info[screen].height,
             screen_info[screen].x_org, screen_info[screen].y_org);
    }

    free(reply);

    if (num_screens == 0) {
        ELOG("No screens found. Please fix your setup. i3 will exit now.\n");
        exit(EXIT_SUCCESS);
    }
}

/*
 * This creates the root_output (borrowed from randr.c) and uses it
 * as the sole output for this session.
 *
 */
static void use_root_output(xcb_connection_t *conn) {
    Output *s = create_root_output(conn);
    s->active = true;
    TAILQ_INSERT_TAIL(&outputs, s, outputs);
    output_init_con(s);
    init_ws_for_output(s);
}

/*
 * We have just established a connection to the X server and need the initial Xinerama
 * information to setup workspaces for each screen.
 *
 */
void xinerama_init(void) {
    if (!xcb_get_extension_data(conn, &xcb_xinerama_id)->present) {
        DLOG("Xinerama extension not found, using root output.\n");
        use_root_output(conn);
    } else {
        xcb_xinerama_is_active_reply_t *reply;
        reply = xcb_xinerama_is_active_reply(conn, xcb_xinerama_is_active(conn), NULL);

        if (reply == NULL || !reply->state) {
            DLOG("Xinerama is not active (in your X-Server), using root output.\n");
            use_root_output(conn);
        } else {
            query_screens(conn);
        }

        FREE(reply);
    }
}
