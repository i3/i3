/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
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
#include "floating.h"
#include "workspace.h"
#include "log.h"

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

        if (client->dock) {
                DLOG("Not putting dock client into floating mode\n");
                return;
        }

        if (con == NULL) {
                DLOG("This client is already in floating (container == NULL), re-inserting\n");
                Client *next_tiling;
                Workspace *ws = client->workspace;
                SLIST_FOREACH(next_tiling, &(ws->focus_stack), focus_clients)
                        if (!client_is_floating(next_tiling))
                                break;
                /* If there are no tiling clients on this workspace, there can only be one
                 * container: the first one */
                if (next_tiling == TAILQ_END(&(ws->focus_stack)))
                        con = ws->table[0][0];
                else con = next_tiling->container;

                /* Remove the client from the list of floating clients */
                TAILQ_REMOVE(&(ws->floating_clients), client, floating_clients);

                DLOG("destination container = %p\n", con);
                Client *old_focused = con->currently_focused;
                /* Preserve position/size */
                memcpy(&(client->floating_rect), &(client->rect), sizeof(Rect));

                client->floating = FLOATING_USER_OFF;
                client->container = con;

                if (old_focused != NULL && !old_focused->dock)
                        CIRCLEQ_INSERT_AFTER(&(con->clients), old_focused, client, clients);
                else CIRCLEQ_INSERT_TAIL(&(con->clients), client, clients);

                DLOG("Re-inserted the window.\n");
                con->currently_focused = client;

                client_set_below_floating(conn, client);

                render_container(conn, con);
                xcb_flush(conn);

                return;
        }

        DLOG("Entering floating for client %08x\n", client->child);

        /* Remove the client of its container */
        client_remove_from_container(conn, client, con, false);
        client->container = NULL;

        /* Add the client to the list of floating clients for its workspace */
        TAILQ_INSERT_TAIL(&(client->workspace->floating_clients), client, floating_clients);

        if (con->currently_focused == client) {
                DLOG("Need to re-adjust currently_focused\n");
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

                DLOG("copying size from tiling (%d, %d) size (%d, %d)\n", client->floating_rect.x, client->floating_rect.y,
                                client->floating_rect.width, client->floating_rect.height);
        } else {
                /* If the client was already in floating before we restore the old position / size */
                DLOG("using: (%d, %d) size (%d, %d)\n", client->floating_rect.x, client->floating_rect.y,
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
 * Removes the floating client from its workspace and attaches it to the new workspace.
 * This is centralized here because it may happen if you move it via keyboard and
 * if you move it using your mouse.
 *
 */
void floating_assign_to_workspace(Client *client, Workspace *new_workspace) {
        /* Remove from focus stack and list of floating clients */
        SLIST_REMOVE(&(client->workspace->focus_stack), client, Client, focus_clients);
        TAILQ_REMOVE(&(client->workspace->floating_clients), client, floating_clients);

        if (client->workspace->fullscreen_client == client)
                client->workspace->fullscreen_client = NULL;

        /* Insert into destination focus stack and list of floating clients */
        client->workspace = new_workspace;
        SLIST_INSERT_HEAD(&(client->workspace->focus_stack), client, focus_clients);
        TAILQ_INSERT_TAIL(&(client->workspace->floating_clients), client, floating_clients);
        if (client->fullscreen)
                client->workspace->fullscreen_client = client;
}

/*
 * This is an ugly data structure which we need because there is no standard
 * way of having nested functions (only available as a gcc extension at the
 * moment, clang doesn’t support it) or blocks (only available as a clang
 * extension and only on Mac OS X systems at the moment).
 *
 */
struct resize_callback_params {
        border_t border;
        xcb_button_press_event_t *event;
};

DRAGGING_CB(resize_callback) {
        struct resize_callback_params *params = extra;
        xcb_button_press_event_t *event = params->event;
        switch (params->border) {
                case BORDER_RIGHT: {
                        int new_width = old_rect->width + (new_x - event->root_x);
                        if ((new_width < 0) ||
                            (new_width < client_min_width(client) && client->rect.width >= new_width))
                                return;
                        client->rect.width = new_width;
                        break;
                }

                case BORDER_BOTTOM: {
                        int new_height = old_rect->height + (new_y - event->root_y);
                        if ((new_height < 0) ||
                            (new_height < client_min_height(client) && client->rect.height >= new_height))
                                return;
                        client->rect.height = old_rect->height + (new_y - event->root_y);
                        break;
                }

                case BORDER_TOP: {
                        int new_height = old_rect->height + (event->root_y - new_y);
                        if ((new_height < 0) ||
                            (new_height < client_min_height(client) && client->rect.height >= new_height))
                                return;

                        client->rect.y = old_rect->y + (new_y - event->root_y);
                        client->rect.height = new_height;
                        break;
                }

                case BORDER_LEFT: {
                        int new_width = old_rect->width + (event->root_x - new_x);
                        if ((new_width < 0) ||
                            (new_width < client_min_width(client) && client->rect.width >= new_width))
                                return;
                        client->rect.x = old_rect->x + (new_x - event->root_x);
                        client->rect.width = new_width;
                        break;
                }
        }

        /* Push the new position/size to X11 */
        reposition_client(conn, client);
        resize_client(conn, client);
        xcb_flush(conn);
}


/*
 * Called whenever the user clicks on a border (not the titlebar!) of a floating window.
 * Determines on which border the user clicked and launches the drag_pointer function
 * with the resize_callback.
 *
 */
int floating_border_click(xcb_connection_t *conn, Client *client, xcb_button_press_event_t *event) {
        DLOG("floating border click\n");

        border_t border;

        if (event->event_y < 2)
                border = BORDER_TOP;
        else if (event->event_y >= (client->rect.height - 2))
                border = BORDER_BOTTOM;
        else if (event->event_x <= 2)
                border = BORDER_LEFT;
        else if (event->event_x >= (client->rect.width - 2))
                border = BORDER_RIGHT;
        else {
                DLOG("Not on any border, not doing anything.\n");
                return 1;
        }

        DLOG("border = %d\n", border);

        struct resize_callback_params params = { border, event };

        drag_pointer(conn, client, event, XCB_NONE, border, resize_callback, &params);

        return 1;
}

DRAGGING_CB(drag_window_callback) {
        struct xcb_button_press_event_t *event = extra;

        /* Reposition the client correctly while moving */
        client->rect.x = old_rect->x + (new_x - event->root_x);
        client->rect.y = old_rect->y + (new_y - event->root_y);
        reposition_client(conn, client);
        /* Because reposition_client does not send a faked configure event (only resize does),
         * we need to initiate that on our own */
        fake_absolute_configure_notify(conn, client);
        /* fake_absolute_configure_notify flushes */
}

/*
 * Called when the user clicked on the titlebar of a floating window.
 * Calls the drag_pointer function with the drag_window callback
 *
 */
void floating_drag_window(xcb_connection_t *conn, Client *client, xcb_button_press_event_t *event) {
        DLOG("floating_drag_window\n");

        drag_pointer(conn, client, event, XCB_NONE, BORDER_TOP /* irrelevant */, drag_window_callback, event);
}

/*
 * This is an ugly data structure which we need because there is no standard
 * way of having nested functions (only available as a gcc extension at the
 * moment, clang doesn’t support it) or blocks (only available as a clang
 * extension and only on Mac OS X systems at the moment).
 *
 */
struct resize_window_callback_params {
        border_t corner;
        xcb_button_press_event_t *event;
};

DRAGGING_CB(resize_window_callback) {
        struct resize_window_callback_params *params = extra;
        xcb_button_press_event_t *event = params->event;
        border_t corner = params->corner;

        int32_t dest_x = client->rect.x;
        int32_t dest_y = client->rect.y;
        uint32_t dest_width;
        uint32_t dest_height;

        if (corner & BORDER_LEFT) {
                dest_x = old_rect->x + (new_x - event->root_x);
                dest_width = old_rect->width - (new_x - event->root_x);
        } else dest_width = old_rect->width + (new_x - event->root_x);

        if (corner & BORDER_TOP) {
                dest_y = old_rect->y + (new_y - event->root_y);
                dest_height = old_rect->height - (new_y - event->root_y);
        } else dest_height = old_rect->height + (new_y - event->root_y);


        /* Obey minimum window size and reposition the client */
        if (dest_width > 0 && dest_width >= client_min_width(client)) {
                client->rect.x = dest_x;
                client->rect.width = dest_width;
        }

        if (dest_height > 0 && dest_height >= client_min_height(client)) {
                client->rect.y = dest_y;
                client->rect.height = dest_height;
        }

        /* resize_client flushes */
        resize_client(conn, client);
}

/*
 * Called when the user clicked on a floating window while holding the
 * floating_modifier and the right mouse button.
 * Calls the drag_pointer function with the resize_window callback
 *
 */
void floating_resize_window(xcb_connection_t *conn, Client *client, xcb_button_press_event_t *event) {
        DLOG("floating_resize_window\n");

        /* corner saves the nearest corner to the original click. It contains
         * a bitmask of the nearest borders (BORDER_LEFT, BORDER_RIGHT, …) */
        border_t corner = 0;

        if (event->event_x <= (client->rect.width / 2))
                corner |= BORDER_LEFT;
        else corner |= BORDER_RIGHT;

        if (event->event_y <= (client->rect.height / 2))
                corner |= BORDER_TOP;
        else corner |= BORDER_RIGHT;

        struct resize_window_callback_params params = { corner, event };

        drag_pointer(conn, client, event, XCB_NONE, BORDER_TOP /* irrelevant */, resize_window_callback, &params);
}


/*
 * This function grabs your pointer and lets you drag stuff around (borders).
 * Every time you move your mouse, an XCB_MOTION_NOTIFY event will be received
 * and the given callback will be called with the parameters specified (client,
 * border on which the click originally was), the original rect of the client,
 * the event and the new coordinates (x, y).
 *
 */
void drag_pointer(xcb_connection_t *conn, Client *client, xcb_button_press_event_t *event,
                  xcb_window_t confine_to, border_t border, callback_t callback, void *extra) {
        xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;
        uint32_t new_x, new_y;
        Rect old_rect;
        if (client != NULL)
                memcpy(&old_rect, &(client->rect), sizeof(Rect));

        /* Grab the pointer */
        /* TODO: returncode */
        xcb_grab_pointer(conn, 
                        false,               /* get all pointer events specified by the following mask */
                        root,                /* grab the root window */
                        XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION, /* which events to let through */
                        XCB_GRAB_MODE_ASYNC, /* pointer events should continue as normal */
                        XCB_GRAB_MODE_ASYNC, /* keyboard mode */
                        confine_to,          /* confine_to = in which window should the cursor stay */
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

                                case XCB_UNMAP_NOTIFY:
                                        DLOG("Unmap-notify, aborting\n");
                                        xcb_event_handle(&evenths, inside_event);
                                        goto done;

                                default:
                                        DLOG("Passing to original handler\n");
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

                callback(conn, client, &old_rect, new_x, new_y, extra);
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
        DLOG("floating focus\n");

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
        DLOG("floating move\n");

        Rect destination = currently_focused->rect;
        Rect *screen = &(currently_focused->workspace->output->rect);

        switch (direction) {
                case D_LEFT:
                        destination.x -= 10;
                        break;
                case D_RIGHT:
                        destination.x += 10;
                        break;
                case D_UP:
                        destination.y -= 10;
                        break;
                case D_DOWN:
                        destination.y += 10;
                        break;
                /* to make static analyzers happy */
                default:
                        break;
        }

        /* Prevent windows from vanishing completely */
        if ((int32_t)(destination.x + destination.width - 5) <= (int32_t)screen->x ||
            (int32_t)(destination.x + 5) >= (int32_t)(screen->x + screen->width) ||
            (int32_t)(destination.y + destination.height - 5) <= (int32_t)screen->y ||
            (int32_t)(destination.y + 5) >= (int32_t)(screen->y + screen->height)) {
                DLOG("boundary check failed, not moving\n");
                return;
        }

        currently_focused->rect = destination;
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
        DLOG("floating_hidden is now: %d\n", workspace->floating_hidden);
        TAILQ_FOREACH(client, &(workspace->floating_clients), floating_clients) {
                if (workspace->floating_hidden)
                        client_unmap(conn, client);
                else client_map(conn, client);
        }

        /* If we just unmapped all floating windows we should ensure that the focus
         * is set correctly, that ist, to the first non-floating client in stack */
        if (workspace->floating_hidden)
                SLIST_FOREACH(client, &(workspace->focus_stack), focus_clients) {
                        if (client_is_floating(client))
                                continue;
                        set_focus(conn, client, true);
                        return;
                }

        xcb_flush(conn);
}
