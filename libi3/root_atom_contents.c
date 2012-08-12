/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>

#include "libi3.h"

/*
 * Try to get the contents of the given atom (for example I3_SOCKET_PATH) from
 * the X11 root window and return NULL if it doesn’t work.
 *
 * The memory for the contents is dynamically allocated and has to be
 * free()d by the caller.
 *
 */
char *root_atom_contents(const char *atomname) {
    xcb_connection_t *conn;
    xcb_intern_atom_cookie_t atom_cookie;
    xcb_intern_atom_reply_t *atom_reply;
    int screen;
    char *content;

    if ((conn = xcb_connect(NULL, &screen)) == NULL ||
        xcb_connection_has_error(conn))
        return NULL;

    atom_cookie = xcb_intern_atom(conn, 0, strlen(atomname), atomname);

    xcb_screen_t *root_screen = xcb_aux_get_screen(conn, screen);
    xcb_window_t root = root_screen->root;

    atom_reply = xcb_intern_atom_reply(conn, atom_cookie, NULL);
    if (atom_reply == NULL)
        return NULL;

    xcb_get_property_cookie_t prop_cookie;
    xcb_get_property_reply_t *prop_reply;
    prop_cookie = xcb_get_property_unchecked(conn, false, root, atom_reply->atom,
                                             XCB_GET_PROPERTY_TYPE_ANY, 0, PATH_MAX);
    prop_reply = xcb_get_property_reply(conn, prop_cookie, NULL);
    if (prop_reply == NULL || xcb_get_property_value_length(prop_reply) == 0)
        return NULL;
    if (prop_reply->type == XCB_ATOM_CARDINAL) {
        /* We treat a CARDINAL as a >= 32-bit unsigned int. The only CARDINAL
         * we query is I3_PID, which is 32-bit. */
        if (asprintf(&content, "%u", *((unsigned int*)xcb_get_property_value(prop_reply))) == -1)
            return NULL;
    } else {
        if (asprintf(&content, "%.*s", xcb_get_property_value_length(prop_reply),
                     (char*)xcb_get_property_value(prop_reply)) == -1)
            return NULL;
    }
    xcb_disconnect(conn);
    return content;
}

