/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * src/floating.c: contains all functions for handling floating clients
 *
 */
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <xcb/xcb.h>
#include <xcb/xcb_event.h>

#include "i3.h"
#include "config.h"
#include "data.h"
#include "util.h"
#include "xcb.h"
#include "debug.h"
#include "layout.h"
#include "client.h"

/* On which border was the dragging initiated? */
typedef enum { BORDER_LEFT, BORDER_RIGHT, BORDER_TOP, BORDER_BOTTOM} border_t;
/* Callback for dragging */
typedef void(*callback_t)(Rect*, uint32_t, uint32_t);

/* Forward definitions */
static void drag_pointer(xcb_connection_t *conn, Client *client, xcb_button_press_event_t *event,
                         border_t border, callback_t callback);

/*
 * Toggles floating mode for the given client.
 * Correctly takes care of the position/size (separately stored for tiling/floating mode)
 * and repositions/resizes/redecorates the client.
 *
 * If the automatic flag is set to true, this was an automatic update by a change of the
 * window class from the application which can be overwritten by the user.
 *
 */
void toggle_floating_mode(xcb_connection_t *conn, Client *client, bool automatic) {
        Container *con = client->container;
        i3Font *font = load_font(conn, config.font);

        if (con == NULL) {
                LOG("This client is already in floating (container == NULL), re-inserting\n");
                Client *next_tiling;
                SLIST_FOREACH(next_tiling, &(client->workspace->focus_stack), focus_clients)
                        if (next_tiling->floating <= FLOATING_USER_OFF)
                                break;
                /* If there are no tiling clients on this workspace, there can only be one
                 * container: the first one */
                if (next_tiling == TAILQ_END(&(client->workspace->focus_stack)))
                        con = client->workspace->table[0][0];
                else con = next_tiling->container;

                /* Remove the client from the list of floating clients */
                TAILQ_REMOVE(&(client->workspace->floating_clients), client, floating_clients);

                LOG("destination container = %p\n", con);
                Client *old_focused = con->currently_focused;
                /* Preserve position/size */
                memcpy(&(client->floating_rect), &(client->rect), sizeof(Rect));

                client->floating = FLOATING_USER_OFF;
                client->container = con;

                if (old_focused != NULL && !old_focused->dock)
                        CIRCLEQ_INSERT_AFTER(&(con->clients), old_focused, client, clients);
                else CIRCLEQ_INSERT_TAIL(&(con->clients), client, clients);

                LOG("Re-inserted the client into the matrix.\n");
                con->currently_focused = client;

                client_set_below_floating(conn, client);

                render_container(conn, con);
                xcb_flush(conn);

                return;
        }

        LOG("Entering floating for client %08x\n", client->child);

        /* Remove the client of its container */
        client_remove_from_container(conn, client, con, false);
        client->container = NULL;

        /* Add the client to the list of floating clients for its workspace */
        TAILQ_INSERT_TAIL(&(client->workspace->floating_clients), client, floating_clients);

        if (con->currently_focused == client) {
                LOG("Need to re-adjust currently_focused\n");
                /* Get the next client in the focus stack for this particular container */
                con->currently_focused = get_last_focused_client(conn, con, NULL);
        }

        if (automatic)
                client->floating = FLOATING_AUTO_ON;
        else client->floating = FLOATING_USER_ON;

        /* Initialize the floating position from the position in tiling mode, if this
         * client never was floating (x == -1) */
        if (client->floating_rect.x == -1) {
                /* Copy over the position */
                client->floating_rect.x = client->rect.x;
                client->floating_rect.y = client->rect.y;

                /* Copy size the other direction */
                client->child_rect.width = client->floating_rect.width;
                client->child_rect.height = client->floating_rect.height;

                client->rect.width = client->child_rect.width + 2 + 2;
                client->rect.height = client->child_rect.height + (font->height + 2 + 2) + 2;

                LOG("copying size from tiling (%d, %d) size (%d, %d)\n", client->floating_rect.x, client->floating_rect.y,
                                client->floating_rect.width, client->floating_rect.height);
        } else {
                /* If the client was already in floating before we restore the old position / size */
                LOG("using: (%d, %d) size (%d, %d)\n", client->floating_rect.x, client->floating_rect.y,
                        client->floating_rect.width, client->floating_rect.height);
                memcpy(&(client->rect), &(client->floating_rect), sizeof(Rect));
        }

        /* Raise the client */
        xcb_raise_window(conn, client->frame);
        reposition_client(conn, client);
        resize_client(conn, client);
        /* redecorate_window flushes */
        redecorate_window(conn, client);

        /* Re-render the tiling layout of this container */
        render_container(conn, con);
        xcb_flush(conn);
}


/*
 * Called whenever the user clicks on a border (not the titlebar!) of a floating window.
 * Determines on which border the user clicked and launches the drag_pointer function
 * with the resize_callback.
 *
 */
int floating_border_click(xcb_connection_t *conn, Client *client, xcb_button_press_event_t *event) {

        LOG("floating border click\n");

        border_t border;

        void resize_callback(Rect *old_rect, uint32_t new_x, uint32_t new_y) {
                switch (border) {
                        case BORDER_RIGHT:
                                client->rect.width = old_rect->width + (new_x - event->root_x);
                                break;

                        case BORDER_BOTTOM:
                                client->rect.height = old_rect->height + (new_y - event->root_y);
                                break;

                        case BORDER_TOP:
                                client->rect.y = old_rect->y + (new_y - event->root_y);
                                client->rect.height = old_rect->height + (event->root_y - new_y);
                                break;

                        case BORDER_LEFT:
                                client->rect.x = old_rect->x + (new_x - event->root_x);
                                client->rect.width = old_rect->width + (event->root_x - new_x);
                                break;
                }

                /* Push the new position/size to X11 */
                reposition_client(conn, client);
                resize_client(conn, client);
                xcb_flush(conn);
        }

        if (event->event_y < 2)
                border = BORDER_TOP;
        else if (event->event_y >= (client->rect.height - 2))
                border = BORDER_BOTTOM;
        else if (event->event_x <= 2)
                border = BORDER_LEFT;
        else if (event->event_x > 2)
                border = BORDER_RIGHT;
        else {
                LOG("Not on any border, not doing anything.\n");
                return 1;
        }

        LOG("border = %d\n", border);

        drag_pointer(conn, client, event, border, resize_callback);

        return 1;
}


/*
 * Called when the user clicked on the titlebar of a floating window.
 * Calls the drag_pointer function with the drag_window callback
 *
 */
void floating_drag_window(xcb_connection_t *conn, Client *client, xcb_button_press_event_t *event) {
        LOG("floating_drag_window\n");

        void drag_window_callback(Rect *old_rect, uint32_t new_x, uint32_t new_y) {
                /* Reposition the client correctly while moving */
                client->rect.x = old_rect->x + (new_x - event->root_x);
                client->rect.y = old_rect->y + (new_y - event->root_y);
                reposition_client(conn, client);
                /* Because reposition_client does not send a faked configure event (only resize does),
                 * we need to initiate that on our own */
                fake_absolute_configure_notify(conn, client);
                /* fake_absolute_configure_notify flushes */
        }


        drag_pointer(conn, client, event, BORDER_TOP /* irrelevant */, drag_window_callback);
}

/*
 * This function grabs your pointer and lets you drag stuff around (borders).
 * Every time you move your mouse, an XCB_MOTION_NOTIFY event will be received
 * and the given callback will be called with the parameters specified (client,
 * border on which the click originally was), the original rect of the client,
 * the event and the new coordinates (x, y).
 *
 */
static void drag_pointer(xcb_connection_t *conn, Client *client, xcb_button_press_event_t *event,
                         border_t border, callback_t callback) {
        xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;
        uint32_t new_x, new_y;
        Rect old_rect;
        memcpy(&old_rect, &(client->rect), sizeof(Rect));

        /* Grab the pointer */
        /* TODO: returncode */
        xcb_grab_pointer(conn, 
                        false,               /* get all pointer events specified by the following mask */
                        root,                /* grab the root window */
                        XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION, /* which events to let through */
                        XCB_GRAB_MODE_ASYNC, /* pointer events should continue as normal */
                        XCB_GRAB_MODE_ASYNC, /* keyboard mode */
                        XCB_NONE,            /* confine_to = in which window should the cursor stay */
                        XCB_NONE,            /* don’t display a special cursor */
                        XCB_CURRENT_TIME);

        /* Go into our own event loop */
        xcb_flush(conn);

        xcb_generic_event_t *inside_event, *last_motion_notify = NULL;
        /* I’ve always wanted to have my own eventhandler… */
        while ((inside_event = xcb_wait_for_event(conn))) {
                /* We now handle all events we can get using xcb_poll_for_event */
                do {
                        /* Same as get_event_handler in xcb */
                        int nr = inside_event->response_type;
                        if (nr == 0) {
                                /* An error occured */
                                handle_event(NULL, conn, inside_event);
                                free(inside_event);
                                continue;
                        }
                        assert(nr < 256);
                        nr &= XCB_EVENT_RESPONSE_TYPE_MASK;
                        assert(nr >= 2);

                        switch (nr) {
                                case XCB_BUTTON_RELEASE:
                                        goto done;

                                case XCB_MOTION_NOTIFY:
                                        /* motion_notify events are saved for later */
                                        FREE(last_motion_notify);
                                        last_motion_notify = inside_event;

                                        break;
                                default:
                                        LOG("Passing to original handler\n");
                                        /* Use original handler */
                                        xcb_event_handle(&evenths, inside_event);
                                        break;
                        }
                        if (last_motion_notify != inside_event)
                                free(inside_event);
                } while ((inside_event = xcb_poll_for_event(conn)) != NULL);

                if (last_motion_notify == NULL)
                        continue;

                new_x = ((xcb_motion_notify_event_t*)last_motion_notify)->root_x;
                new_y = ((xcb_motion_notify_event_t*)last_motion_notify)->root_y;

                callback(&old_rect, new_x, new_y);
                FREE(last_motion_notify);
        }
done:
        xcb_ungrab_pointer(conn, XCB_CURRENT_TIME);
        xcb_flush(conn);
}

/*
 * Changes focus in the given direction for floating clients.
 *
 * Changing to the left/right means going to the previous/next floating client,
 * changing to top/bottom means cycling through the Z-index.
 *
 */
void floating_focus_direction(xcb_connection_t *conn, Client *currently_focused, direction_t direction) {
        LOG("floating focus\n");

        if (direction == D_LEFT || direction == D_RIGHT) {
                /* Go to the next/previous floating client */
                Client *client;

                while ((client = (direction == D_LEFT ? TAILQ_PREV(currently_focused, floating_clients_head, floating_clients) :
                                                        TAILQ_NEXT(currently_focused, floating_clients))) !=
                       TAILQ_END(&(currently_focused->workspace->floating_clients))) {
                        if (!client->floating)
                                continue;
                        set_focus(conn, client, true);
                        return;
                }
        }
}

/*
 * Moves the client 10px to the specified direction.
 *
 */
void floating_move(xcb_connection_t *conn, Client *currently_focused, direction_t direction) {
        LOG("floating move\n");

        switch (direction) {
                case D_LEFT:
                        if (currently_focused->rect.x < 10)
                                return;
                        currently_focused->rect.x -= 10;
                        break;
                case D_RIGHT:
                        currently_focused->rect.x += 10;
                        break;
                case D_UP:
                        if (currently_focused->rect.y < 10)
                                return;
                        currently_focused->rect.y -= 10;
                        break;
                case D_DOWN:
                        currently_focused->rect.y += 10;
                        break;
                /* to make static analyzers happy */
                default:
                        break;
        }

        reposition_client(conn, currently_focused);

        /* Because reposition_client does not send a faked configure event (only resize does),
         * we need to initiate that on our own */
        fake_absolute_configure_notify(conn, currently_focused);
        /* fake_absolute_configure_notify flushes */
}

/*
 * Hides all floating clients (or show them if they are currently hidden) on
 * the specified workspace.
 *
 */
void floating_toggle_hide(xcb_connection_t *conn, Workspace *workspace) {
        Client *client;

        workspace->floating_hidden = !workspace->floating_hidden;
        LOG("floating_hidden is now: %d\n", workspace->floating_hidden);
        TAILQ_FOREACH(client, &(workspace->floating_clients), floating_clients) {
                if (workspace->floating_hidden)
                        xcb_unmap_window(conn, client->frame);
                else xcb_map_window(conn, client->frame);
        }

        /* If we just unmapped all floating windows we should ensure that the focus
         * is set correctly, that ist, to the first non-floating client in stack */
        if (workspace->floating_hidden)
                SLIST_FOREACH(client, &(workspace->focus_stack), focus_clients)
                        if (client->floating <= FLOATING_USER_OFF) {
                                set_focus(conn, client, true);
                                return;
                        }

        xcb_flush(conn);
}
