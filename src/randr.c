/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2011 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * For more information on RandR, please see the X.org RandR specification at
 * http://cgit.freedesktop.org/xorg/proto/randrproto/tree/randrproto.txt
 * (take your time to read it completely, it answers all questions).
 *
 */
#include <time.h>

#include <xcb/randr.h>

#include "all.h"

/* While a clean namespace is usually a pretty good thing, we really need
 * to use shorter names than the whole xcb_randr_* default names. */
typedef xcb_randr_get_crtc_info_reply_t crtc_info;
typedef xcb_randr_mode_info_t mode_info;
typedef xcb_randr_get_screen_resources_current_reply_t resources_reply;

/* Pointer to the result of the query for primary output */
xcb_randr_get_output_primary_reply_t *primary;

/* Stores all outputs available in your current session. */
struct outputs_head outputs = TAILQ_HEAD_INITIALIZER(outputs);

static bool randr_disabled = false;

/*
 * Get a specific output by its internal X11 id. Used by randr_query_outputs
 * to check if the output is new (only in the first scan) or if we are
 * re-scanning.
 *
 */
static Output *get_output_by_id(xcb_randr_output_t id) {
    Output *output;
    TAILQ_FOREACH(output, &outputs, outputs)
        if (output->id == id)
            return output;

    return NULL;
}

/*
 * Returns the output with the given name if it is active (!) or NULL.
 *
 */
Output *get_output_by_name(const char *name) {
    Output *output;
    TAILQ_FOREACH(output, &outputs, outputs)
        if (output->active &&
            strcasecmp(output->name, name) == 0)
            return output;

    return NULL;
}

/*
 * Returns the first output which is active.
 *
 */
Output *get_first_output() {
    Output *output;

    TAILQ_FOREACH(output, &outputs, outputs)
        if (output->active)
            return output;

    return NULL;
}

/*
 * Returns the active (!) output which contains the coordinates x, y or NULL
 * if there is no output which contains these coordinates.
 *
 */
Output *get_output_containing(int x, int y) {
    Output *output;
    TAILQ_FOREACH(output, &outputs, outputs) {
        if (!output->active)
            continue;
        DLOG("comparing x=%d y=%d with x=%d and y=%d width %d height %d\n",
                        x, y, output->rect.x, output->rect.y, output->rect.width, output->rect.height);
        if (x >= output->rect.x && x < (output->rect.x + output->rect.width) &&
            y >= output->rect.y && y < (output->rect.y + output->rect.height))
            return output;
    }

    return NULL;
}

/*
 * Gets the output which is the last one in the given direction, for example
 * the output on the most bottom when direction == D_DOWN, the output most
 * right when direction == D_RIGHT and so on.
 *
 * This function always returns a output.
 *
 */
Output *get_output_most(direction_t direction, Output *current) {
    Output *output, *candidate = NULL;
    int position = 0;
    TAILQ_FOREACH(output, &outputs, outputs) {
        if (!output->active)
            continue;

        /* Repeated calls of WIN determine the winner of the comparison */
        #define WIN(variable, condition) \
            if (variable condition) { \
                candidate = output; \
                position = variable; \
            } \
            break;

        if (((direction == D_UP) || (direction == D_DOWN)) &&
            (current->rect.x != output->rect.x))
            continue;

        if (((direction == D_LEFT) || (direction == D_RIGHT)) &&
            (current->rect.y != output->rect.y))
            continue;

        switch (direction) {
            case D_UP:
                WIN(output->rect.y, <= position);
            case D_DOWN:
                WIN(output->rect.y, >= position);
            case D_LEFT:
                WIN(output->rect.x, <= position);
            case D_RIGHT:
                WIN(output->rect.x, >= position);
        }
    }

    assert(candidate != NULL);

    return candidate;
}

#if 0
/*
 * Initializes the specified output, assigning the specified workspace to it.
 *
 */
void initialize_output(xcb_connection_t *conn, Output *output, Workspace *workspace) {
        i3Font *font = load_font(conn, config.font);

        workspace->output = output;
        output->current_workspace = workspace;

        /* Copy rect for the workspace */
        memcpy(&(workspace->rect), &(output->rect), sizeof(Rect));

        /* Map clients on the workspace, if any */
        workspace_map_clients(conn, workspace);

        /* Create a bar window on each output */
        if (!config.disable_workspace_bar) {
                Rect bar_rect = {output->rect.x,
                                 output->rect.y + output->rect.height - (font->height + 6),
                                 output->rect.x + output->rect.width,
                                 font->height + 6};
                uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
                uint32_t values[] = {1, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS};
                output->bar = create_window(conn, bar_rect, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_CURSOR_LEFT_PTR, true, mask, values);
                output->bargc = xcb_generate_id(conn);
                xcb_create_gc(conn, output->bargc, output->bar, 0, 0);
        }

        SLIST_INIT(&(output->dock_clients));

        ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"init\"}");
        DLOG("initialized output at (%d, %d) with %d x %d\n",
                        output->rect.x, output->rect.y, output->rect.width, output->rect.height);

        DLOG("assigning configured workspaces to this output...\n");
        Workspace *ws;
        TAILQ_FOREACH(ws, workspaces, workspaces) {
                if (ws == workspace)
                        continue;
                if (ws->preferred_output == NULL ||
                    get_output_by_name(ws->preferred_output) != output)
                        continue;

                DLOG("assigning ws %d\n", ws->num + 1);
                workspace_assign_to(ws, output, true);
        }
}
#endif

/*
 * Disables RandR support by creating exactly one output with the size of the
 * X11 screen.
 *
 */
void disable_randr(xcb_connection_t *conn) {
    xcb_screen_t *root_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

    DLOG("RandR extension unusable, disabling.\n");

    Output *s = scalloc(sizeof(Output));

    s->active = true;
    s->rect.x = 0;
    s->rect.y = 0;
    s->rect.width = root_screen->width_in_pixels;
    s->rect.height = root_screen->height_in_pixels;
    s->name = "xroot-0";
    output_init_con(s);

    TAILQ_INSERT_TAIL(&outputs, s, outputs);

    randr_disabled = true;
}

/*
 * Initializes a CT_OUTPUT Con (searches existing ones from inplace restart
 * before) to use for the given Output.
 *
 * XXX: for assignments, we probably need to move workspace creation from here
 * to after the loop in randr_query_outputs().
 *
 */
void output_init_con(Output *output) {
    Con *con = NULL, *current;
    bool reused = false;

    DLOG("init_con for output %s\n", output->name);

    /* Search for a Con with that name directly below the root node. There
     * might be one from a restored layout. */
    TAILQ_FOREACH(current, &(croot->nodes_head), nodes) {
        if (strcmp(current->name, output->name) != 0)
            continue;

        con = current;
        reused = true;
        DLOG("Using existing con %p / %s\n", con, con->name);
        break;
    }

    if (con == NULL) {
        con = con_new(croot);
        FREE(con->name);
        con->name = sstrdup(output->name);
        con->type = CT_OUTPUT;
        con->layout = L_OUTPUT;
    }
    con->rect = output->rect;
    output->con = con;

    char *name;
    asprintf(&name, "[i3 con] output %s", con->name);
    x_set_name(con, name);
    FREE(name);

    if (reused) {
        DLOG("Not adding workspace, this was a reused con\n");
        return;
    }

    DLOG("Changing layout, adding top/bottom dockarea\n");
    Con *topdock = con_new(NULL);
    topdock->type = CT_DOCKAREA;
    topdock->layout = L_DOCKAREA;
    topdock->orientation = VERT;
    /* this container swallows dock clients */
    Match *match = scalloc(sizeof(Match));
    match_init(match);
    match->dock = M_DOCK_TOP;
    match->insert_where = M_BELOW;
    TAILQ_INSERT_TAIL(&(topdock->swallow_head), match, matches);

    topdock->name = sstrdup("topdock");

    asprintf(&name, "[i3 con] top dockarea %s", con->name);
    x_set_name(topdock, name);
    FREE(name);
    DLOG("attaching\n");
    con_attach(topdock, con, false);

    /* content container */

    DLOG("adding main content container\n");
    Con *content = con_new(NULL);
    content->type = CT_CON;
    content->name = sstrdup("content");

    asprintf(&name, "[i3 con] content %s", con->name);
    x_set_name(content, name);
    FREE(name);
    con_attach(content, con, false);

    /* bottom dock container */
    Con *bottomdock = con_new(NULL);
    bottomdock->type = CT_DOCKAREA;
    bottomdock->layout = L_DOCKAREA;
    bottomdock->orientation = VERT;
    /* this container swallows dock clients */
    match = scalloc(sizeof(Match));
    match_init(match);
    match->dock = M_DOCK_BOTTOM;
    match->insert_where = M_BELOW;
    TAILQ_INSERT_TAIL(&(bottomdock->swallow_head), match, matches);

    bottomdock->name = sstrdup("bottomdock");

    asprintf(&name, "[i3 con] bottom dockarea %s", con->name);
    x_set_name(bottomdock, name);
    FREE(name);
    DLOG("attaching\n");
    con_attach(bottomdock, con, false);

    DLOG("Now adding a workspace\n");

    /* add a workspace to this output */
    Con *ws = con_new(NULL);
    ws->type = CT_WORKSPACE;

    /* get the next unused workspace number */
    DLOG("Getting next unused workspace\n");
    int c = 0;
    bool exists = true;
    while (exists) {
        Con *out, *current, *child;

        c++;

        FREE(ws->name);
        asprintf(&(ws->name), "%d", c);

        exists = false;
        TAILQ_FOREACH(out, &(croot->nodes_head), nodes) {
            TAILQ_FOREACH(current, &(out->nodes_head), nodes) {
                if (current->type != CT_CON)
                    continue;

                TAILQ_FOREACH(child, &(current->nodes_head), nodes) {
                    if (strcasecmp(child->name, ws->name) != 0)
                        continue;

                    exists = true;
                    break;
                }
            }
        }

        DLOG("result for ws %s / %d: exists = %d\n", ws->name, c, exists);
    }
    ws->num = c;
    con_attach(ws, content, false);

    asprintf(&name, "[i3 con] workspace %s", ws->name);
    x_set_name(ws, name);
    free(name);

    ws->fullscreen_mode = CF_OUTPUT;
    ws->orientation = HORIZ;

    /* TODO: Set focus in main.c */
    con_focus(ws);
}

/*
 * This function needs to be called when changing the mode of an output when
 * it already has some workspaces (or a bar window) assigned.
 *
 * It reconfigures the bar window for the new mode, copies the new rect into
 * each workspace on this output and forces all windows on the affected
 * workspaces to be reconfigured.
 *
 * It is necessary to call render_layout() afterwards.
 *
 */
static void output_change_mode(xcb_connection_t *conn, Output *output) {
    //i3Font *font = load_font(conn, config.font);

    DLOG("Output mode changed, updating rect\n");
    assert(output->con != NULL);
    output->con->rect = output->rect;
#if 0
    Rect bar_rect = {output->rect.x,
                     output->rect.y + output->rect.height - (font->height + 6),
                     output->rect.x + output->rect.width,
                     font->height + 6};

    xcb_set_window_rect(conn, output->bar, bar_rect);

        /* go through all workspaces and set force_reconfigure */
        TAILQ_FOREACH(ws, workspaces, workspaces) {
                if (ws->output != output)
                        continue;

                SLIST_FOREACH(client, &(ws->focus_stack), focus_clients) {
                        client->force_reconfigure = true;
                        if (!client_is_floating(client))
                                continue;
                        /* For floating clients we need to translate the
                         * coordinates (old workspace to new workspace) */
                        DLOG("old: (%x, %x)\n", client->rect.x, client->rect.y);
                        client->rect.x -= ws->rect.x;
                        client->rect.y -= ws->rect.y;
                        client->rect.x += ws->output->rect.x;
                        client->rect.y += ws->output->rect.y;
                        DLOG("new: (%x, %x)\n", client->rect.x, client->rect.y);
                }

                /* Update dimensions from output */
                memcpy(&(ws->rect), &(ws->output->rect), sizeof(Rect));

                /* Update the dimensions of a fullscreen client, if any */
                if (ws->fullscreen_client != NULL) {
                        DLOG("Updating fullscreen client size\n");
                        client = ws->fullscreen_client;
                        Rect r = ws->rect;
                        xcb_set_window_rect(conn, client->frame, r);

                        r.x = 0;
                        r.y = 0;
                        xcb_set_window_rect(conn, client->child, r);
                }
        }
#endif
}

/*
 * Gets called by randr_query_outputs() for each output. The function adds new
 * outputs to the list of outputs, checks if the mode of existing outputs has
 * been changed or if an existing output has been disabled. It will then change
 * either the "changed" or the "to_be_deleted" flag of the output, if
 * appropriate.
 *
 */
static void handle_output(xcb_connection_t *conn, xcb_randr_output_t id,
                          xcb_randr_get_output_info_reply_t *output,
                          xcb_timestamp_t cts, resources_reply *res) {
    /* each CRT controller has a position in which we are interested in */
    crtc_info *crtc;

    Output *new = get_output_by_id(id);
    bool existing = (new != NULL);
    if (!existing)
        new = scalloc(sizeof(Output));
    new->id = id;
    new->primary = (primary && primary->output == id);
    FREE(new->name);
    asprintf(&new->name, "%.*s",
            xcb_randr_get_output_info_name_length(output),
            xcb_randr_get_output_info_name(output));

    DLOG("found output with name %s\n", new->name);

    /* Even if no CRTC is used at the moment, we store the output so that
     * we do not need to change the list ever again (we only update the
     * position/size) */
    if (output->crtc == XCB_NONE) {
        if (!existing) {
            if (new->primary)
                TAILQ_INSERT_HEAD(&outputs, new, outputs);
            else TAILQ_INSERT_TAIL(&outputs, new, outputs);
        } else if (new->active)
            new->to_be_disabled = true;
        return;
    }

    xcb_randr_get_crtc_info_cookie_t icookie;
    icookie = xcb_randr_get_crtc_info(conn, output->crtc, cts);
    if ((crtc = xcb_randr_get_crtc_info_reply(conn, icookie, NULL)) == NULL) {
        DLOG("Skipping output %s: could not get CRTC (%p)\n",
             new->name, crtc);
        free(new);
        return;
    }

    bool updated = update_if_necessary(&(new->rect.x), crtc->x) |
                   update_if_necessary(&(new->rect.y), crtc->y) |
                   update_if_necessary(&(new->rect.width), crtc->width) |
                   update_if_necessary(&(new->rect.height), crtc->height);
    free(crtc);
    new->active = (new->rect.width != 0 && new->rect.height != 0);
    if (!new->active) {
        DLOG("width/height 0/0, disabling output\n");
        return;
    }

    DLOG("mode: %dx%d+%d+%d\n", new->rect.width, new->rect.height,
                                new->rect.x, new->rect.y);

    /* If we don’t need to change an existing output or if the output
     * does not exist in the first place, the case is simple: we either
     * need to insert the new output or we are done. */
    if (!updated || !existing) {
        if (!existing) {
            if (new->primary)
                TAILQ_INSERT_HEAD(&outputs, new, outputs);
            else TAILQ_INSERT_TAIL(&outputs, new, outputs);
        }
        return;
    }

    new->changed = true;
}

/*
 * (Re-)queries the outputs via RandR and stores them in the list of outputs.
 *
 */
void randr_query_outputs() {
    Output *output, *other, *first;
    xcb_randr_get_output_primary_cookie_t pcookie;
    xcb_randr_get_screen_resources_current_cookie_t rcookie;
    resources_reply *res;

    /* timestamp of the configuration so that we get consistent replies to all
     * requests (if the configuration changes between our different calls) */
    xcb_timestamp_t cts;

    /* an output is VGA-1, LVDS-1, etc. (usually physical video outputs) */
    xcb_randr_output_t *randr_outputs;

    if (randr_disabled)
        return;

    /* Get screen resources (primary output, crtcs, outputs, modes) */
    rcookie = xcb_randr_get_screen_resources_current(conn, root);
    pcookie = xcb_randr_get_output_primary(conn, root);

    if ((primary = xcb_randr_get_output_primary_reply(conn, pcookie, NULL)) == NULL)
        ELOG("Could not get RandR primary output\n");
    else DLOG("primary output is %08x\n", primary->output);
    if ((res = xcb_randr_get_screen_resources_current_reply(conn, rcookie, NULL)) == NULL) {
        disable_randr(conn);
        return;
    }
    cts = res->config_timestamp;

    int len = xcb_randr_get_screen_resources_current_outputs_length(res);
    randr_outputs = xcb_randr_get_screen_resources_current_outputs(res);

    /* Request information for each output */
    xcb_randr_get_output_info_cookie_t ocookie[len];
    for (int i = 0; i < len; i++)
        ocookie[i] = xcb_randr_get_output_info(conn, randr_outputs[i], cts);

    /* Loop through all outputs available for this X11 screen */
    for (int i = 0; i < len; i++) {
        xcb_randr_get_output_info_reply_t *output;

        if ((output = xcb_randr_get_output_info_reply(conn, ocookie[i], NULL)) == NULL)
            continue;

        handle_output(conn, randr_outputs[i], output, cts, res);
        free(output);
    }

    /* Check for clones, disable the clones and reduce the mode to the
     * lowest common mode */
    TAILQ_FOREACH(output, &outputs, outputs) {
        if (!output->active || output->to_be_disabled)
            continue;
        DLOG("output %p / %s, position (%d, %d), checking for clones\n",
                output, output->name, output->rect.x, output->rect.y);

        for (other = output;
             other != TAILQ_END(&outputs);
             other = TAILQ_NEXT(other, outputs)) {
            if (other == output || !other->active || other->to_be_disabled)
                continue;

            if (other->rect.x != output->rect.x ||
                other->rect.y != output->rect.y)
                continue;

            DLOG("output %p has the same position, his mode = %d x %d\n",
                            other, other->rect.width, other->rect.height);
            uint32_t width = min(other->rect.width, output->rect.width);
            uint32_t height = min(other->rect.height, output->rect.height);

            if (update_if_necessary(&(output->rect.width), width) |
                update_if_necessary(&(output->rect.height), height))
                output->changed = true;

            update_if_necessary(&(other->rect.width), width);
            update_if_necessary(&(other->rect.height), height);

            DLOG("disabling output %p (%s)\n", other, other->name);
            other->to_be_disabled = true;

            DLOG("new output mode %d x %d, other mode %d x %d\n",
                            output->rect.width, output->rect.height,
                            other->rect.width, other->rect.height);
        }
    }

    /* Handle outputs which have a new mode or are disabled now (either
     * because the user disabled them or because they are clones) */
    TAILQ_FOREACH(output, &outputs, outputs) {
        if (output->to_be_disabled) {
            output->active = false;
            DLOG("Output %s disabled, re-assigning workspaces/docks\n", output->name);

            if ((first = get_first_output()) == NULL)
                die("No usable outputs available\n");

            Con *first_content = output_get_content(first->con);

            if (output->con != NULL) {
                /* We need to move the workspaces from the disappearing output to the first output */
                /* 1: Get the con to focus next, if the disappearing ws is focused */
                Con *next = NULL;
                if (TAILQ_FIRST(&(croot->focus_head)) == output->con) {
                    DLOG("This output (%p) was focused! Getting next\n", output->con);
                    next = con_next_focused(output->con);
                    DLOG("next = %p\n", next);
                }

                /* 2: iterate through workspaces and re-assign them */
                Con *current;
                Con *old_content = output_get_content(output->con);
                while (!TAILQ_EMPTY(&(old_content->nodes_head))) {
                    current = TAILQ_FIRST(&(old_content->nodes_head));
                    DLOG("Detaching current = %p / %s\n", current, current->name);
                    con_detach(current);
                    DLOG("Re-attaching current = %p / %s\n", current, current->name);
                    con_attach(current, first_content, false);
                    DLOG("Done, next\n");
                }
                DLOG("re-attached all workspaces\n");

                if (next) {
                    DLOG("now focusing next = %p\n", next);
                    con_focus(next);
                }

                DLOG("destroying disappearing con %p\n", output->con);
                tree_close(output->con, false, true);
                DLOG("Done. Should be fine now\n");
                output->con = NULL;
            }

            output->to_be_disabled = false;
            output->changed = false;
        }

        if (output->active && output->con == NULL) {
            DLOG("Need to initialize a Con for output %s\n", output->name);
            output_init_con(output);
            output->changed = false;
        }

        if (output->changed) {
            output_change_mode(conn, output);
            output->changed = false;
        }
    }

    if (TAILQ_EMPTY(&outputs)) {
        ELOG("No outputs found via RandR, disabling\n");
        disable_randr(conn);
    }

    //ewmh_update_workarea();

#if 0
    /* Just go through each active output and associate one workspace */
    TAILQ_FOREACH(output, &outputs, outputs) {
            if (!output->active || output->current_workspace != NULL)
                    continue;
            ws = get_first_workspace_for_output(output);
            initialize_output(conn, output, ws);
    }
#endif

    /* Focus the primary screen, if possible */
    TAILQ_FOREACH(output, &outputs, outputs) {
        if (!output->primary || !output->con)
            continue;

        DLOG("Focusing primary output %s\n", output->name);
        con_focus(con_descend_focused(output->con));
    }

    /* render_layout flushes */
    tree_render();

    FREE(res);
    FREE(primary);
}

/*
 * We have just established a connection to the X server and need the initial
 * XRandR information to setup workspaces for each screen.
 *
 */
void randr_init(int *event_base) {
    const xcb_query_extension_reply_t *extreply;

    extreply = xcb_get_extension_data(conn, &xcb_randr_id);
    if (!extreply->present)
        disable_randr(conn);
    else randr_query_outputs();

    if (event_base != NULL)
        *event_base = extreply->first_event;

    xcb_randr_select_input(conn, root,
            XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE |
            XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE |
            XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE |
            XCB_RANDR_NOTIFY_MASK_OUTPUT_PROPERTY);

    xcb_flush(conn);
}
