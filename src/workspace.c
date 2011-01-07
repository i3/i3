/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2010 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * workspace.c: Functions for modifying workspaces
 *
 */
#include <limits.h>

#include "all.h"

/*
 * Returns a pointer to the workspace with the given number (starting at 0),
 * creating the workspace if necessary (by allocating the necessary amount of
 * memory and initializing the data structures correctly).
 *
 */
Con *workspace_get(const char *num) {
    Con *output, *workspace = NULL, *current;

    /* TODO: could that look like this in the future?
    GET_MATCHING_NODE(workspace, croot, strcasecmp(current->name, num) != 0);
    */
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes) {
        TAILQ_FOREACH(current, &(output->nodes_head), nodes) {
            if (strcasecmp(current->name, num) != 0)
                continue;

            workspace = current;
            break;
        }
    }

    LOG("getting ws %s\n", num);
    if (workspace == NULL) {
        LOG("need to create this one\n");
        output = con_get_output(focused);
        LOG("got output %p\n", output);
        /* We need to attach this container after setting its type. con_attach
         * will handle CT_WORKSPACEs differently */
        workspace = con_new(NULL);
        char *name;
        asprintf(&name, "[i3 con] workspace %s", num);
        x_set_name(workspace, name);
        free(name);
        workspace->type = CT_WORKSPACE;
        FREE(workspace->name);
        workspace->name = sstrdup(num);
        /* We set ->num to the number if this workspace’s name consists only of
         * a positive number. Otherwise it’s a named ws and num will be -1. */
        char *end;
        long parsed_num = strtol(num, &end, 10);
        if (parsed_num == LONG_MIN ||
            parsed_num == LONG_MAX ||
            parsed_num < 0 ||
            (end && *end != '\0'))
            workspace->num = -1;
        else workspace->num = parsed_num;
        LOG("num = %d\n", workspace->num);
        workspace->orientation = HORIZ;
        con_attach(workspace, output, false);

        ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"init\"}");
    }

    //ewmh_update_workarea();

    return workspace;
}

#if 0

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
#endif

/*
 * Returns true if the workspace is currently visible. Especially important for
 * multi-monitor environments, as they can have multiple currenlty active
 * workspaces.
 *
 */
bool workspace_is_visible(Con *ws) {
    Con *output = con_get_output(ws);
    if (output == NULL)
        return false;
    Con *fs = con_get_fullscreen_con(output);
    LOG("workspace visible? fs = %p, ws = %p\n", fs, ws);
    return (fs == ws);
}

/*
 * XXX: we need to clean up all this recursive walking code.
 *
 */
Con *_get_sticky(Con *con, const char *sticky_group, Con *exclude) {
    Con *current;

    TAILQ_FOREACH(current, &(con->nodes_head), nodes) {
        if (current != exclude &&
            current->sticky_group != NULL &&
            current->window != NULL &&
            strcmp(current->sticky_group, sticky_group) == 0)
            return current;

        Con *recurse = _get_sticky(current, sticky_group, exclude);
        if (recurse != NULL)
            return recurse;
    }

    TAILQ_FOREACH(current, &(con->floating_head), floating_windows) {
        if (current != exclude &&
            current->sticky_group != NULL &&
            current->window != NULL &&
            strcmp(current->sticky_group, sticky_group) == 0)
            return current;

        Con *recurse = _get_sticky(current, sticky_group, exclude);
        if (recurse != NULL)
            return recurse;
    }

    return NULL;
}

/*
 * Reassigns all child windows in sticky containers. Called when the user
 * changes workspaces.
 *
 * XXX: what about sticky containers which contain containers?
 *
 */
static void workspace_reassign_sticky(Con *con) {
    Con *current;
    /* 1: go through all containers */

    /* handle all children and floating windows of this node */
    TAILQ_FOREACH(current, &(con->nodes_head), nodes) {
        if (current->sticky_group == NULL) {
            workspace_reassign_sticky(current);
            continue;
        }

        LOG("Ah, this one is sticky: %s / %p\n", current->name, current);
        /* 2: find a window which we can re-assign */
        Con *output = con_get_output(current);
        Con *src = _get_sticky(output, current->sticky_group, current);

        if (src == NULL) {
            LOG("No window found for this sticky group\n");
            workspace_reassign_sticky(current);
            continue;
        }

        x_move_win(src, current);
        current->window = src->window;
        current->mapped = true;
        src->window = NULL;
        src->mapped = false;

        x_reparent_child(current, src);

        LOG("re-assigned window from src %p to dest %p\n", src, current);
    }

    TAILQ_FOREACH(current, &(con->floating_head), floating_windows)
        workspace_reassign_sticky(current);
}

/*
 * Switches to the given workspace
 *
 */
void workspace_show(const char *num) {
    Con *workspace, *current, *old;

    workspace = workspace_get(num);

    /* disable fullscreen for the other workspaces and get the workspace we are
     * currently on. */
    TAILQ_FOREACH(current, &(workspace->parent->nodes_head), nodes) {
        if (current->fullscreen_mode == CF_OUTPUT)
            old = current;
        current->fullscreen_mode = CF_NONE;
    }

    /* enable fullscreen for the target workspace. If it happens to be the
     * same one we are currently on anyways, we can stop here. */
    workspace->fullscreen_mode = CF_OUTPUT;
    if (workspace == old)
        return;
    /* disable fullscreen */
    TAILQ_FOREACH(current, &(workspace->parent->nodes_head), nodes)
        current->fullscreen_mode = CF_NONE;

    workspace_reassign_sticky(workspace);

    LOG("switching to %p\n", workspace);
    Con *next = workspace;

    while (!TAILQ_EMPTY(&(next->focus_head)))
        next = TAILQ_FIRST(&(next->focus_head));


    if (TAILQ_EMPTY(&(old->nodes_head)) && TAILQ_EMPTY(&(old->floating_head))) {
        /* check if this workspace is currently visible */
        if (!workspace_is_visible(old)) {
            LOG("Closing old workspace (%p / %s), it is empty\n", old, old->name);
            tree_close(old, false, false);
            ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"empty\"}");
        }
    }

    con_focus(next);
    workspace->fullscreen_mode = CF_OUTPUT;
    LOG("focused now = %p / %s\n", focused, focused->name);

    ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"focus\"}");
#if 0

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
#endif
}

#if 0
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

        /* however, if this is the current workspace, we only need to adjust
         * the output’s current_workspace pointer (and must not unmap the
         * windows) */
        if (c_ws == ws) {
                DLOG("Need to adjust output->current_workspace...\n");
                output->current_workspace = c_ws;
                ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"focus\"}");
                return;
        }

        workspace_unmap_clients(global_conn, ws);
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

#endif

static bool get_urgency_flag(Con *con) {
    Con *child;
    TAILQ_FOREACH(child, &(con->nodes_head), nodes)
        if (child->urgent || get_urgency_flag(child))
            return true;

    TAILQ_FOREACH(child, &(con->floating_head), floating_windows)
        if (child->urgent || get_urgency_flag(child))
            return true;

    return false;
}

/*
 * Goes through all clients on the given workspace and updates the workspace’s
 * urgent flag accordingly.
 *
 */
void workspace_update_urgent_flag(Con *ws) {
    bool old_flag = ws->urgent;
    ws->urgent = get_urgency_flag(ws);
    DLOG("Workspace urgency flag changed from %d to %d\n", old_flag, ws->urgent);

    if (old_flag != ws->urgent)
        ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"urgent\"}");
}
