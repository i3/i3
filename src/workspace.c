/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * workspace.c: Functions for modifying workspaces
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <err.h>

#include "util.h"
#include "data.h"
#include "i3.h"
#include "config.h"
#include "xcb.h"
#include "table.h"
#include "xinerama.h"
#include "layout.h"
#include "workspace.h"
#include "client.h"

/*
 * Returns a pointer to the workspace with the given number (starting at 0),
 * creating the workspace if necessary (by allocating the necessary amount of
 * memory and initializing the data structures correctly).
 *
 */
Workspace *workspace_get(int number) {
        if (number > (num_workspaces-1)) {
                int old_num_workspaces = num_workspaces;

                /* Convert all container->workspace and client->workspace
                 * pointers to numbers representing their workspace. Necessary
                 * because the realloc() may make all the pointers invalid, so
                 * we need to preserve them this way and restore them later.
                 *
                 * To distinguish between the first workspace and a NULL
                 * pointer, we store <workspace number> + 1. */
                for (int c = 0; c < num_workspaces; c++)
                        FOR_TABLE(&(workspaces[c])) {
                                Container *con = workspaces[c].table[cols][rows];
                                if (con->workspace != NULL) {
                                        LOG("Handling con %p with pointer %p (num %d)\n", con, con->workspace, con->workspace->num);
                                        con->workspace = (Workspace*)(con->workspace->num + 1);
                                }
                                Client *current;
                                SLIST_FOREACH(current, &(workspaces[c].focus_stack), focus_clients) {
                                        if (current->workspace == NULL)
                                                continue;
                                        LOG("Handling client %p with pointer %p (num %d)\n", current, current->workspace, current->workspace->num);
                                        current->workspace = (Workspace*)(current->workspace->num + 1);
                                }
                        }

                /* preserve c_ws */
                c_ws = (Workspace*)(c_ws->num);

                LOG("We need to initialize that one\n");
                num_workspaces = number+1;
                workspaces = realloc(workspaces, num_workspaces * sizeof(Workspace));
                /* Zero out the new workspaces so that we have sane default values */
                for (int c = old_num_workspaces; c < num_workspaces; c++)
                        memset(&workspaces[c], 0, sizeof(Workspace));

                /* Immediately after the realloc(), we restore the pointers.
                 * They may be used when initializing the new workspaces, for
                 * example when the user configures containers to be stacking
                 * by default, thus requiring re-rendering the layout. */
                c_ws = workspace_get((int)c_ws);

                for (int c = 0; c < old_num_workspaces; c++)
                        FOR_TABLE(&(workspaces[c])) {
                                Container *con = workspaces[c].table[cols][rows];
                                if (con->workspace != NULL) {
                                        LOG("Handling con %p with (num %d)\n", con, con->workspace);
                                        con->workspace = workspace_get((int)con->workspace - 1);
                                }
                                Client *current;
                                SLIST_FOREACH(current, &(workspaces[c].focus_stack), focus_clients) {
                                        if (current->workspace == NULL)
                                                continue;
                                        LOG("Handling client %p with (num %d)\n", current, current->workspace);
                                        current->workspace = workspace_get((int)current->workspace - 1);
                                }
                        }

                /* Initialize the new workspaces */
                for (int c = old_num_workspaces; c < num_workspaces; c++) {
                        memset(&workspaces[c], 0, sizeof(Workspace));
                        workspaces[c].num = c;
                        TAILQ_INIT(&(workspaces[c].floating_clients));
                        expand_table_cols(&(workspaces[c]));
                        expand_table_rows(&(workspaces[c]));
                        workspace_set_name(&(workspaces[c]), NULL);
                }

                LOG("done\n");
        }

        return &(workspaces[number]);
}

/*
 * Sets the name (or just its number) for the given workspace. This has to
 * be called for every workspace as the rendering function
 * (render_internal_bar) relies on workspace->name and workspace->name_len
 * being ready-to-use.
 *
 */
void workspace_set_name(Workspace *ws, const char *name) {
        char *label;
        int ret;

        if (name != NULL)
                ret = asprintf(&label, "%d: %s", ws->num + 1, name);
        else ret = asprintf(&label, "%d", ws->num + 1);

        if (ret == -1)
                errx(1, "asprintf() failed");

        FREE(ws->name);

        ws->name = convert_utf8_to_ucs2(label, &(ws->name_len));
        if (config.font != NULL)
                ws->text_width = predict_text_width(global_conn, config.font, ws->name, ws->name_len);
        else ws->text_width = 0;

        free(label);
}

/*
 * Returns true if the workspace is currently visible. Especially important for
 * multi-monitor environments, as they can have multiple currenlty active
 * workspaces.
 *
 */
bool workspace_is_visible(Workspace *ws) {
        return (ws->screen->current_workspace == ws->num);
}

/*
 * Switches to the given workspace
 *
 */
void workspace_show(xcb_connection_t *conn, int workspace) {
        bool need_warp = false;
        xcb_window_t root = xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;
        /* t_ws (to workspace) is just a convenience pointer to the workspace we’re switching to */
        Workspace *t_ws = workspace_get(workspace-1);

        LOG("show_workspace(%d)\n", workspace);

        /* Store current_row/current_col */
        c_ws->current_row = current_row;
        c_ws->current_col = current_col;

        /* Check if the workspace has not been used yet */
        workspace_initialize(t_ws, c_ws->screen);

        if (c_ws->screen != t_ws->screen) {
                /* We need to switch to the other screen first */
                LOG("moving over to other screen.\n");

                /* Store the old client */
                Client *old_client = CUR_CELL->currently_focused;

                c_ws = workspace_get(t_ws->screen->current_workspace);
                current_col = c_ws->current_col;
                current_row = c_ws->current_row;
                if (CUR_CELL->currently_focused != NULL)
                        need_warp = true;
                else {
                        Rect *dims = &(c_ws->screen->rect);
                        xcb_warp_pointer(conn, XCB_NONE, root, 0, 0, 0, 0,
                                         dims->x + (dims->width / 2), dims->y + (dims->height / 2));
                }

                /* Re-decorate the old client, it’s not focused anymore */
                if ((old_client != NULL) && !old_client->dock)
                        redecorate_window(conn, old_client);
                else xcb_flush(conn);
        }

        /* Check if we need to change something or if we’re already there */
        if (c_ws->screen->current_workspace == (workspace-1)) {
                Client *last_focused = SLIST_FIRST(&(c_ws->focus_stack));
                if (last_focused != SLIST_END(&(c_ws->focus_stack)))
                        set_focus(conn, last_focused, true);
                if (need_warp) {
                        client_warp_pointer_into(conn, last_focused);
                        xcb_flush(conn);
                }

                return;
        }

        t_ws->screen->current_workspace = workspace-1;
        Workspace *old_workspace = c_ws;
        c_ws = workspace_get(workspace-1);

        /* Unmap all clients of the old workspace */
        workspace_unmap_clients(conn, old_workspace);

        current_row = c_ws->current_row;
        current_col = c_ws->current_col;
        LOG("new current row = %d, current col = %d\n", current_row, current_col);

        workspace_map_clients(conn, c_ws);

        /* POTENTIAL TO IMPROVE HERE: due to the call to _map_clients first and
         * render_layout afterwards, there is a short flickering on the source
         * workspace (assign ws 3 to screen 0, ws 4 to screen 1, create single
         * client on ws 4, move it to ws 3, switch to ws 3, you’ll see the
         * flickering). */

        /* Restore focus on the new workspace */
        Client *last_focused = SLIST_FIRST(&(c_ws->focus_stack));
        if (last_focused != SLIST_END(&(c_ws->focus_stack)))
                set_focus(conn, last_focused, true);
        else xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, root, XCB_CURRENT_TIME);

        render_layout(conn);

        /* We can warp the pointer only after the window has been
         * reconfigured in render_layout, otherwise the pointer will
         * be warped to the old position, which will not work when we
         * moved it to another screen. */
        if (last_focused != SLIST_END(&(c_ws->focus_stack)) && need_warp) {
                client_warp_pointer_into(conn, last_focused);
                xcb_flush(conn);
        }
}


/*
 * Parses the preferred_screen property of a workspace. You can either specify
 * the screen number (it is not given that the screen numbering always stays
 * the same) or the screen coordinates (exact coordinates, e.g. 1280 will match
 * the screen starting at x=1280, but 1281 will not). For coordinates, you can
 * either specify an x coordinate ("1280") or an y coordinate ("x800") or both
 * ("1280x800").
 *
 */
static i3Screen *get_screen_from_preference(struct screens_head *slist, char *preference) {
        i3Screen *screen;
        char *rest;
        int preferred_screen = strtol(preference, &rest, 10);

        LOG("Getting screen for preference \"%s\" (%d)\n", preference, preferred_screen);

        if ((rest == preference) || (preferred_screen >= num_screens)) {
                int x = INT_MAX, y = INT_MAX;
                if (strchr(preference, 'x') != NULL) {
                        /* Check if only the y coordinate was specified */
                        if (*preference == 'x')
                                y = atoi(preference+1);
                        else {
                                x = atoi(preference);
                                y = atoi(strchr(preference, 'x') + 1);
                        }
                } else {
                        x = atoi(preference);
                }

                LOG("Looking for screen at %d x %d\n", x, y);

                TAILQ_FOREACH(screen, slist, screens)
                        if ((x == INT_MAX || screen->rect.x == x) &&
                            (y == INT_MAX || screen->rect.y == y)) {
                                LOG("found %p\n", screen);
                                return screen;
                        }

                LOG("none found\n");
                return NULL;
        } else {
                int c = 0;
                TAILQ_FOREACH(screen, slist, screens)
                        if (c++ == preferred_screen)
                                return screen;
        }

        return NULL;
}

/*
 * Initializes the given workspace if it is not already initialized. The given
 * screen is to be understood as a fallback, if the workspace itself either
 * was not assigned to a particular screen or cannot be placed there because
 * the screen is not attached at the moment.
 *
 */
void workspace_initialize(Workspace *ws, i3Screen *screen) {
        if (ws->screen != NULL) {
                LOG("Workspace already initialized\n");
                return;
        }

        /* If this workspace has no preferred screen or if the screen it wants
         * to be on is not available at the moment, we initialize it with
         * the screen which was given */
        if (ws->preferred_screen == NULL ||
            (ws->screen = get_screen_from_preference(virtual_screens, ws->preferred_screen)) == NULL)
                ws->screen = screen;

        /* Copy the dimensions from the virtual screen */
        memcpy(&(ws->rect), &(ws->screen->rect), sizeof(Rect));
}

/*
 * Gets the first unused workspace for the given screen, taking into account
 * the preferred_screen setting of every workspace (workspace assignments).
 *
 */
Workspace *get_first_workspace_for_screen(struct screens_head *slist, i3Screen *screen) {
        Workspace *result = NULL;

        for (int c = 0; c < num_workspaces; c++) {
                Workspace *ws = workspace_get(c);
                if (ws->preferred_screen == NULL ||
                    !screens_are_equal(get_screen_from_preference(slist, ws->preferred_screen), screen))
                        continue;

                result = ws;
                break;
        }

        if (result == NULL) {
                /* No assignment found, returning first unused workspace */
                for (int c = 0; c < num_workspaces; c++) {
                        if (workspaces[c].screen != NULL)
                                continue;

                        result = workspace_get(c);
                        break;
                }
        }

        if (result == NULL) {
                LOG("No existing free workspace found to assign, creating a new one\n");

                result = workspace_get(num_workspaces);
        }

        workspace_initialize(result, screen);
        return result;
}

/*
 * Maps all clients (and stack windows) of the given workspace.
 *
 */
void workspace_map_clients(xcb_connection_t *conn, Workspace *ws) {
        Client *client;

        ignore_enter_notify_forall(conn, ws, true);

        /* Map all clients on the new workspace */
        FOR_TABLE(ws)
                CIRCLEQ_FOREACH(client, &(ws->table[cols][rows]->clients), clients)
                        client_map(conn, client);

        /* Map all floating clients */
        if (!ws->floating_hidden)
                TAILQ_FOREACH(client, &(ws->floating_clients), floating_clients)
                        client_map(conn, client);

        /* Map all stack windows, if any */
        struct Stack_Window *stack_win;
        SLIST_FOREACH(stack_win, &stack_wins, stack_windows)
                if (stack_win->container->workspace == ws)
                        xcb_map_window(conn, stack_win->window);

        ignore_enter_notify_forall(conn, ws, false);
}

/*
 * Unmaps all clients (and stack windows) of the given workspace.
 *
 * This needs to be called separately when temporarily rendering
 * a workspace which is not the active workspace to force
 * reconfiguration of all clients, like in src/xinerama.c when
 * re-assigning a workspace to another screen.
 *
 */
void workspace_unmap_clients(xcb_connection_t *conn, Workspace *u_ws) {
        Client *client;
        struct Stack_Window *stack_win;

        /* Ignore notify events because they would cause focus to be changed */
        ignore_enter_notify_forall(conn, u_ws, true);

        /* Unmap all clients of the given workspace */
        int unmapped_clients = 0;
        FOR_TABLE(u_ws)
                CIRCLEQ_FOREACH(client, &(u_ws->table[cols][rows]->clients), clients) {
                        LOG("unmapping normal client %p / %p / %p\n", client, client->frame, client->child);
                        client_unmap(conn, client);
                        unmapped_clients++;
                }

        /* To find floating clients, we traverse the focus stack */
        SLIST_FOREACH(client, &(u_ws->focus_stack), focus_clients) {
                if (!client_is_floating(client))
                        continue;

                LOG("unmapping floating client %p / %p / %p\n", client, client->frame, client->child);

                client_unmap(conn, client);
                unmapped_clients++;
        }

        /* If we did not unmap any clients, the workspace is empty and we can destroy it, at least
         * if it is not the current workspace. */
        if (unmapped_clients == 0 && u_ws != c_ws) {
                /* Re-assign the workspace of all dock clients which use this workspace */
                Client *dock;
                LOG("workspace %p is empty\n", u_ws);
                SLIST_FOREACH(dock, &(u_ws->screen->dock_clients), dock_clients) {
                        if (dock->workspace != u_ws)
                                continue;

                        LOG("Re-assigning dock client to c_ws (%p)\n", c_ws);
                        dock->workspace = c_ws;
                }
                u_ws->screen = NULL;
        }

        /* Unmap the stack windows on the given workspace, if any */
        SLIST_FOREACH(stack_win, &stack_wins, stack_windows)
                if (stack_win->container->workspace == u_ws)
                        xcb_unmap_window(conn, stack_win->window);

        ignore_enter_notify_forall(conn, u_ws, false);
}

/*
 * Goes through all clients on the given workspace and updates the workspace’s
 * urgent flag accordingly.
 *
 */
void workspace_update_urgent_flag(Workspace *ws) {
        Client *current;

        SLIST_FOREACH(current, &(ws->focus_stack), focus_clients) {
                if (!current->urgent)
                        continue;

                ws->urgent = true;
                return;
        }

        ws->urgent = false;
}
