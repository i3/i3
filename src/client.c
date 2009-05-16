/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * client.c: holds all client-specific functions
 *
 */
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

#include "data.h"
#include "i3.h"
#include "xcb.h"
#include "util.h"
#include "queue.h"

/*
 * Removes the given client from the container, either because it will be inserted into another
 * one or because it was unmapped
 *
 */
void client_remove_from_container(xcb_connection_t *conn, Client *client, Container *container) {
        CIRCLEQ_REMOVE(&(container->clients), client, clients);

        SLIST_REMOVE(&(container->workspace->focus_stack), client, Client, focus_clients);

        /* If the container will be empty now and is in stacking mode, we need to
           unmap the stack_win */
        if (CIRCLEQ_EMPTY(&(container->clients)) && container->mode == MODE_STACK) {
                struct Stack_Window *stack_win = &(container->stack_win);
                stack_win->rect.height = 0;
                xcb_unmap_window(conn, stack_win->window);
        }
}

/*
 * Warps the pointer into the given client (in the middle of it, to be specific), therefore
 * selecting it
 *
 */
void client_warp_pointer_into(xcb_connection_t *conn, Client *client) {
        int mid_x = client->rect.width / 2,
            mid_y = client->rect.height / 2;
        xcb_warp_pointer(conn, XCB_NONE, client->child, 0, 0, 0, 0, mid_x, mid_y);
}

/*
 * Returns true if the client supports the given protocol atom (like WM_DELETE_WINDOW)
 *
 */
static bool client_supports_protocol(xcb_connection_t *conn, Client *client, xcb_atom_t atom) {
        xcb_get_property_cookie_t cookie;
        xcb_get_wm_protocols_reply_t protocols;
        bool result = false;

        cookie = xcb_get_wm_protocols_unchecked(conn, client->child, atoms[WM_PROTOCOLS]);
        if (xcb_get_wm_protocols_reply(conn, cookie, &protocols, NULL) != 1)
                return false;

        /* Check if the client’s protocols have the requested atom set */
        for (uint32_t i = 0; i < protocols.atoms_len; i++)
                if (protocols.atoms[i] == atom)
                        result = true;

        xcb_get_wm_protocols_reply_wipe(&protocols);

        return result;
}

/*
 * Kills the given window using WM_DELETE_WINDOW or xcb_kill_window
 *
 */
void client_kill(xcb_connection_t *conn, Client *window) {
        /* If the client does not support WM_DELETE_WINDOW, we kill it the hard way */
        if (!client_supports_protocol(conn, window, atoms[WM_DELETE_WINDOW])) {
                LOG("Killing window the hard way\n");
                xcb_kill_client(conn, window->child);
                return;
        }

        xcb_client_message_event_t ev;

        memset(&ev, 0, sizeof(xcb_client_message_event_t));

        ev.response_type = XCB_CLIENT_MESSAGE;
        ev.window = window->child;
        ev.type = atoms[WM_PROTOCOLS];
        ev.format = 32;
        ev.data.data32[0] = atoms[WM_DELETE_WINDOW];
        ev.data.data32[1] = XCB_CURRENT_TIME;

        LOG("Sending WM_DELETE to the client\n");
        xcb_send_event(conn, false, window->child, XCB_EVENT_MASK_NO_EVENT, (char*)&ev);
        xcb_flush(conn);
}

/*
 * Checks if the given window class and title match the given client
 * Window title is passed as "normal" string and as UCS-2 converted string for
 * matching _NET_WM_NAME capable clients as well as those using legacy hints.
 *
 */
bool client_matches_class_name(Client *client, char *to_class, char *to_title,
                               char *to_title_ucs, int to_title_ucs_len) {
        /* Check if the given class is part of the window class */
        if (strcasestr(client->window_class, to_class) == NULL)
                return false;

        /* If no title was given, we’re done */
        if (to_title == NULL)
                return true;

        if (client->name_len > -1) {
                /* UCS-2 converted window titles */
                if (memmem(client->name, (client->name_len * 2), to_title_ucs, (to_title_ucs_len * 2)) == NULL)
                        return false;
        } else {
                /* Legacy hints */
                if (strcasestr(client->name, to_title) == NULL)
                        return false;
        }

        return true;
}
