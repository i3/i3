/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
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
#include "randr.h"
#include "layout.h"
#include "workspace.h"
#include "client.h"
#include "log.h"
#include "ewmh.h"
#include "ipc.h"

/*
 * Returns a pointer to the workspace with the given number (starting at 0),
 * creating the workspace if necessary (by allocating the necessary amount of
 * memory and initializing the data structures correctly).
 *
 */
Workspace *workspace_get(int number) {
        Workspace *ws = NULL;
        TAILQ_FOREACH(ws, workspaces, workspaces)
                if (ws->num == number)
                        return ws;

        /* If we are still there, we could not find the requested workspace. */
        int last_ws = TAILQ_LAST(workspaces, workspaces_head)->num;

        DLOG("We need to initialize that one, last ws = %d\n", last_ws);

        for (int c = last_ws; c < number; c++) {
                DLOG("Creating new ws\n");

                ws = scalloc(sizeof(Workspace));
                ws->num = c+1;
                TAILQ_INIT(&(ws->floating_clients));
                expand_table_cols(ws);
                expand_table_rows(ws);
                workspace_set_name(ws, NULL);

                TAILQ_INSERT_TAIL(workspaces, ws, workspaces);

                ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"init\"}");
        }
        DLOG("done\n");

        ewmh_update_workarea();

        return ws;
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
        FREE(ws->utf8_name);

        ws->name = convert_utf8_to_ucs2(label, &(ws->name_len));
        if (config.font != NULL)
                ws->text_width = predict_text_width(global_conn, config.font, ws->name, ws->name_len);
        else ws->text_width = 0;
        ws->utf8_name = label;
}

/*
 * Returns true if the workspace is currently visible. Especially important for
 * multi-monitor environments, as they can have multiple currenlty active
 * workspaces.
 *
 */
bool workspace_is_visible(Workspace *ws) {
        return (ws->output->current_workspace == ws);
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

        DLOG("show_workspace(%d)\n", workspace);

        /* Store current_row/current_col */
        c_ws->current_row = current_row;
        c_ws->current_col = current_col;

        /* Check if the workspace has not been used yet */
        workspace_initialize(t_ws, c_ws->output, false);

        if (c_ws->output != t_ws->output) {
                /* We need to switch to the other output first */
                DLOG("moving over to other output.\n");

                /* Store the old client */
                Client *old_client = CUR_CELL->currently_focused;

                c_ws = t_ws->output->current_workspace;
                current_col = c_ws->current_col;
                current_row = c_ws->current_row;
                if (CUR_CELL->currently_focused != NULL)
                        need_warp = true;
                else {
                        Rect *dims = &(c_ws->output->rect);
                        xcb_warp_pointer(conn, XCB_NONE, root, 0, 0, 0, 0,
                                         dims->x + (dims->width / 2), dims->y + (dims->height / 2));
                }

                /* Re-decorate the old client, it’s not focused anymore */
                if ((old_client != NULL) && !old_client->dock)
                        redecorate_window(conn, old_client);
                else xcb_flush(conn);

                /* We need to check if a global fullscreen-client is blocking
                 * the t_ws and if necessary switch that to local fullscreen */
                Client* client = c_ws->fullscreen_client;
                if (client != NULL && client->workspace != c_ws) {
                        if (c_ws->fullscreen_client->workspace != c_ws)
                                c_ws->fullscreen_client = NULL;
                        client_enter_fullscreen(conn, client, false);
                }
        }

        /* Check if we need to change something or if we’re already there */
        if (c_ws->output->current_workspace->num == (workspace-1)) {
                Client *last_focused = SLIST_FIRST(&(c_ws->focus_stack));
                if (last_focused != SLIST_END(&(c_ws->focus_stack)))
                        set_focus(conn, last_focused, true);
                if (need_warp) {
                        client_warp_pointer_into(conn, last_focused);
                        xcb_flush(conn);
                }

                ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"focus\"}");

                return;
        }

        Workspace *old_workspace = c_ws;
        c_ws = t_ws->output->current_workspace = workspace_get(workspace-1);

        /* Unmap all clients of the old workspace */
        workspace_unmap_clients(conn, old_workspace);

        current_row = c_ws->current_row;
        current_col = c_ws->current_col;
        DLOG("new current row = %d, current col = %d\n", current_row, current_col);

        ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"focus\"}");

        workspace_map_clients(conn, c_ws);

        /* POTENTIAL TO IMPROVE HERE: due to the call to _map_clients first and
         * render_layout afterwards, there is a short flickering on the source
         * workspace (assign ws 3 to output 0, ws 4 to output 1, create single
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
         * moved it to another output. */
        if (last_focused != SLIST_END(&(c_ws->focus_stack)) && need_warp) {
                client_warp_pointer_into(conn, last_focused);
                xcb_flush(conn);
        }
}

/*
 * Assigns the given workspace to the given output by correctly updating its
 * state and reconfiguring all the clients on this workspace.
 *
 * This is called when initializing a output and when re-assigning it to a
 * different output which just got available (if you configured it to be on
 * output 1 and you just plugged in output 1).
 *
 */
void workspace_assign_to(Workspace *ws, Output *output, bool hide_it) {
        Client *client;
        bool empty = true;
        bool visible = workspace_is_visible(ws);

        ws->output = output;

        /* Copy the dimensions from the virtual output */
        memcpy(&(ws->rect), &(ws->output->rect), sizeof(Rect));

        ewmh_update_workarea();

        /* Force reconfiguration for each client on that workspace */
        SLIST_FOREACH(client, &(ws->focus_stack), focus_clients) {
                client->force_reconfigure = true;
                empty = false;
        }

        if (empty)
                return;

        /* Render the workspace to reconfigure the clients. However, they will be visible now, so… */
        render_workspace(global_conn, output, ws);

        /* …unless we want to see them at the moment, we should hide that workspace */
        if (visible && !hide_it)
                return;

        workspace_unmap_clients(global_conn, ws);

        if (c_ws == ws) {
                DLOG("Need to adjust output->current_workspace...\n");
                output->current_workspace = c_ws;
        }
}

/*
 * Initializes the given workspace if it is not already initialized. The given
 * screen is to be understood as a fallback, if the workspace itself either
 * was not assigned to a particular screen or cannot be placed there because
 * the screen is not attached at the moment.
 *
 */
void workspace_initialize(Workspace *ws, Output *output, bool recheck) {
        Output *old_output;

        if (ws->output != NULL && !recheck) {
                DLOG("Workspace already initialized\n");
                return;
        }

        old_output = ws->output;

        /* If this workspace has no preferred output or if the output it wants
         * to be on is not available at the moment, we initialize it with
         * the output which was given */
        if (ws->preferred_output == NULL ||
            (ws->output = get_output_by_name(ws->preferred_output)) == NULL)
                ws->output = output;

        DLOG("old_output = %p, ws->output = %p\n", old_output, ws->output);
        /* If the assignment did not change, we do not need to update anything */
        if (old_output != NULL && ws->output == old_output)
                return;

        workspace_assign_to(ws, ws->output, false);
}

/*
 * Gets the first unused workspace for the given screen, taking into account
 * the preferred_output setting of every workspace (workspace assignments).
 *
 */
Workspace *get_first_workspace_for_output(Output *output) {
        Workspace *result = NULL;

        Workspace *ws;
        TAILQ_FOREACH(ws, workspaces, workspaces) {
                if (ws->preferred_output == NULL ||
                    get_output_by_name(ws->preferred_output) != output)
                        continue;

                result = ws;
                break;
        }

        if (result == NULL) {
                /* No assignment found, returning first unused workspace */
                TAILQ_FOREACH(ws, workspaces, workspaces) {
                        if (ws->output != NULL)
                                continue;

                        result = ws;
                        break;
                }
        }

        if (result == NULL) {
                DLOG("No existing free workspace found to assign, creating a new one\n");

                int last_ws = 0;
                TAILQ_FOREACH(ws, workspaces, workspaces)
                        last_ws = ws->num;
                result = workspace_get(last_ws + 1);
        }

        workspace_initialize(result, output, false);
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
                if (stack_win->container->workspace == ws && stack_win->rect.height > 0)
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
                        DLOG("unmapping normal client %p / %p / %p\n", client, client->frame, client->child);
                        client_unmap(conn, client);
                        unmapped_clients++;
                }

        /* To find floating clients, we traverse the focus stack */
        SLIST_FOREACH(client, &(u_ws->focus_stack), focus_clients) {
                if (!client_is_floating(client))
                        continue;

                DLOG("unmapping floating client %p / %p / %p\n", client, client->frame, client->child);

                client_unmap(conn, client);
                unmapped_clients++;
        }

        /* If we did not unmap any clients, the workspace is empty and we can destroy it, at least
         * if it is not the current workspace. */
        if (unmapped_clients == 0 && u_ws != c_ws) {
                /* Re-assign the workspace of all dock clients which use this workspace */
                Client *dock;
                DLOG("workspace %p is empty\n", u_ws);
                SLIST_FOREACH(dock, &(u_ws->output->dock_clients), dock_clients) {
                        if (dock->workspace != u_ws)
                                continue;

                        DLOG("Re-assigning dock client to c_ws (%p)\n", c_ws);
                        dock->workspace = c_ws;
                }
                u_ws->output = NULL;
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

/*
 * Returns the width of the workspace.
 *
 */
int workspace_width(Workspace *ws) {
        return ws->rect.width;
}

/*
 * Returns the effective height of the workspace (without the internal bar and
 * without dock clients).
 *
 */
int workspace_height(Workspace *ws) {
        int height = ws->rect.height;
        i3Font *font = load_font(global_conn, config.font);

        /* Reserve space for dock clients */
        Client *client;
        SLIST_FOREACH(client, &(ws->output->dock_clients), dock_clients)
                height -= client->desired_height;

        /* Space for the internal bar */
        height -= (font->height + 6);

        return height;
}
