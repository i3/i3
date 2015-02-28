#undef I3__FILE__
#define I3__FILE__ "xinerama.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
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
    TAILQ_FOREACH(output, &outputs, outputs)
    if (output->rect.x == x && output->rect.y == y)
        return output;

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
            s = scalloc(sizeof(Output));
            sasprintf(&(s->name), "xinerama-%d", num_screens);
            DLOG("Created new Xinerama screen %s (%p)\n", s->name, s);
            s->active = true;
            s->rect.x = screen_info[screen].x_org;
            s->rect.y = screen_info[screen].y_org;
            s->rect.width = screen_info[screen].width;
            s->rect.height = screen_info[screen].height;
            /* We always treat the screen at 0x0 as the primary screen */
            if (s->rect.x == 0 && s->rect.y == 0)
                TAILQ_INSERT_HEAD(&outputs, s, outputs);
            else
                TAILQ_INSERT_TAIL(&outputs, s, outputs);
            output_init_con(s);
            init_ws_for_output(s, output_get_content(s->con));
            num_screens++;
        }

        DLOG("found Xinerama screen: %d x %d at %d x %d\n",
             screen_info[screen].width, screen_info[screen].height,
             screen_info[screen].x_org, screen_info[screen].y_org);
    }

    free(reply);

    if (num_screens == 0) {
        ELOG("No screens found. Please fix your setup. i3 will exit now.\n");
        exit(0);
    }
}

/*
 * We have just established a connection to the X server and need the initial Xinerama
 * information to setup workspaces for each screen.
 *
 */
void xinerama_init(void) {
    if (!xcb_get_extension_data(conn, &xcb_xinerama_id)->present) {
        DLOG("Xinerama extension not found, disabling.\n");
        disable_randr(conn);
    } else {
        xcb_xinerama_is_active_reply_t *reply;
        reply = xcb_xinerama_is_active_reply(conn, xcb_xinerama_is_active(conn), NULL);

        if (reply == NULL || !reply->state) {
            DLOG("Xinerama is not active (in your X-Server), disabling.\n");
            disable_randr(conn);
        } else
            query_screens(conn);

        FREE(reply);
    }

#if 0
    Output *output;
    Workspace *ws;
    /* Just go through each active output and associate one workspace */
    TAILQ_FOREACH(output, &outputs, outputs) {
        ws = get_first_workspace_for_output(output);
        initialize_output(conn, output, ws);
    }
#endif
}
