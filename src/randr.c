#undef I3__FILE__
#define I3__FILE__ "randr.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * For more information on RandR, please see the X.org RandR specification at
 * http://cgit.freedesktop.org/xorg/proto/randrproto/tree/randrproto.txt
 * (take your time to read it completely, it answers all questions).
 *
 */
#include "all.h"

#include <time.h>
#include <xcb/randr.h>

/* While a clean namespace is usually a pretty good thing, we really need
 * to use shorter names than the whole xcb_randr_* default names. */
typedef xcb_randr_get_crtc_info_reply_t crtc_info;
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
Output *get_first_output(void) {
    Output *output;

    TAILQ_FOREACH(output, &outputs, outputs)
    if (output->active)
        return output;

    die("No usable outputs available.\n");
}

/*
 * Returns the active (!) output which contains the coordinates x, y or NULL
 * if there is no output which contains these coordinates.
 *
 */
Output *get_output_containing(unsigned int x, unsigned int y) {
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
 * In contained_by_output, we check if any active output contains part of the container.
 * We do this by checking if the output rect is intersected by the Rect.
 * This is the 2-dimensional counterpart of get_output_containing.
 * Since we don't actually need the outputs intersected by the given Rect (There could
 * be many), we just return true or false for convenience.
 *
 */
bool contained_by_output(Rect rect) {
    Output *output;
    int lx = rect.x, uy = rect.y;
    int rx = rect.x + rect.width, by = rect.y + rect.height;
    TAILQ_FOREACH(output, &outputs, outputs) {
        if (!output->active)
            continue;
        DLOG("comparing x=%d y=%d with x=%d and y=%d width %d height %d\n",
             rect.x, rect.y, output->rect.x, output->rect.y, output->rect.width, output->rect.height);
        if (rx >= (int)output->rect.x && lx <= (int)(output->rect.x + output->rect.width) &&
            by >= (int)output->rect.y && uy <= (int)(output->rect.y + output->rect.height))
            return true;
    }
    return false;
}

/*
 * Like get_output_next with close_far == CLOSEST_OUTPUT, but wraps.
 *
 * For example if get_output_next(D_DOWN, x, FARTHEST_OUTPUT) = NULL, then
 * get_output_next_wrap(D_DOWN, x) will return the topmost output.
 *
 * This function always returns a output: if no active outputs can be found,
 * current itself is returned.
 *
 */
Output *get_output_next_wrap(direction_t direction, Output *current) {
    Output *best = get_output_next(direction, current, CLOSEST_OUTPUT);
    /* If no output can be found, wrap */
    if (!best) {
        direction_t opposite;
        if (direction == D_RIGHT)
            opposite = D_LEFT;
        else if (direction == D_LEFT)
            opposite = D_RIGHT;
        else if (direction == D_DOWN)
            opposite = D_UP;
        else
            opposite = D_DOWN;
        best = get_output_next(opposite, current, FARTHEST_OUTPUT);
    }
    if (!best)
        best = current;
    DLOG("current = %s, best = %s\n", current->name, best->name);
    return best;
}

/*
 * Gets the output which is the next one in the given direction.
 *
 * If close_far == CLOSEST_OUTPUT, then the output next to the current one will
 * selected. If close_far == FARTHEST_OUTPUT, the output which is the last one
 * in the given direction will be selected.
 *
 * NULL will be returned when no active outputs are present in the direction
 * specified (note that “current” counts as such an output).
 *
 */
Output *get_output_next(direction_t direction, Output *current, output_close_far_t close_far) {
    Rect *cur = &(current->rect),
         *other;
    Output *output,
        *best = NULL;
    TAILQ_FOREACH(output, &outputs, outputs) {
        if (!output->active)
            continue;

        other = &(output->rect);

        if ((direction == D_RIGHT && other->x > cur->x) ||
            (direction == D_LEFT && other->x < cur->x)) {
            /* Skip the output when it doesn’t overlap the other one’s y
             * coordinate at all. */
            if ((other->y + other->height) <= cur->y ||
                (cur->y + cur->height) <= other->y)
                continue;
        } else if ((direction == D_DOWN && other->y > cur->y) ||
                   (direction == D_UP && other->y < cur->y)) {
            /* Skip the output when it doesn’t overlap the other one’s x
             * coordinate at all. */
            if ((other->x + other->width) <= cur->x ||
                (cur->x + cur->width) <= other->x)
                continue;
        } else
            continue;

        /* No candidate yet? Start with this one. */
        if (!best) {
            best = output;
            continue;
        }

        if (close_far == CLOSEST_OUTPUT) {
            /* Is this output better (closer to the current output) than our
             * current best bet? */
            if ((direction == D_RIGHT && other->x < best->rect.x) ||
                (direction == D_LEFT && other->x > best->rect.x) ||
                (direction == D_DOWN && other->y < best->rect.y) ||
                (direction == D_UP && other->y > best->rect.y)) {
                best = output;
                continue;
            }
        } else {
            /* Is this output better (farther to the current output) than our
             * current best bet? */
            if ((direction == D_RIGHT && other->x > best->rect.x) ||
                (direction == D_LEFT && other->x < best->rect.x) ||
                (direction == D_DOWN && other->y > best->rect.y) ||
                (direction == D_UP && other->y < best->rect.y)) {
                best = output;
                continue;
            }
        }
    }

    DLOG("current = %s, best = %s\n", current->name, (best ? best->name : "NULL"));
    return best;
}

/*
 * Disables RandR support by creating exactly one output with the size of the
 * X11 screen.
 *
 */
void disable_randr(xcb_connection_t *conn) {
    DLOG("RandR extension unusable, disabling.\n");

    Output *s = scalloc(sizeof(Output));

    s->active = true;
    s->rect.x = 0;
    s->rect.y = 0;
    s->rect.width = root_screen->width_in_pixels;
    s->rect.height = root_screen->height_in_pixels;
    s->name = "xroot-0";
    output_init_con(s);
    init_ws_for_output(s, output_get_content(s->con));

    TAILQ_INSERT_TAIL(&outputs, s, outputs);

    randr_disabled = true;
}

/*
 * Initializes a CT_OUTPUT Con (searches existing ones from inplace restart
 * before) to use for the given Output.
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
        con = con_new(croot, NULL);
        FREE(con->name);
        con->name = sstrdup(output->name);
        con->type = CT_OUTPUT;
        con->layout = L_OUTPUT;
        con_fix_percent(croot);
    }
    con->rect = output->rect;
    output->con = con;

    char *name;
    sasprintf(&name, "[i3 con] output %s", con->name);
    x_set_name(con, name);
    FREE(name);

    if (reused) {
        DLOG("Not adding workspace, this was a reused con\n");
        return;
    }

    DLOG("Changing layout, adding top/bottom dockarea\n");
    Con *topdock = con_new(NULL, NULL);
    topdock->type = CT_DOCKAREA;
    topdock->layout = L_DOCKAREA;
    /* this container swallows dock clients */
    Match *match = scalloc(sizeof(Match));
    match_init(match);
    match->dock = M_DOCK_TOP;
    match->insert_where = M_BELOW;
    TAILQ_INSERT_TAIL(&(topdock->swallow_head), match, matches);

    FREE(topdock->name);
    topdock->name = sstrdup("topdock");

    sasprintf(&name, "[i3 con] top dockarea %s", con->name);
    x_set_name(topdock, name);
    FREE(name);
    DLOG("attaching\n");
    con_attach(topdock, con, false);

    /* content container */

    DLOG("adding main content container\n");
    Con *content = con_new(NULL, NULL);
    content->type = CT_CON;
    content->layout = L_SPLITH;
    FREE(content->name);
    content->name = sstrdup("content");

    sasprintf(&name, "[i3 con] content %s", con->name);
    x_set_name(content, name);
    FREE(name);
    con_attach(content, con, false);

    /* bottom dock container */
    Con *bottomdock = con_new(NULL, NULL);
    bottomdock->type = CT_DOCKAREA;
    bottomdock->layout = L_DOCKAREA;
    /* this container swallows dock clients */
    match = scalloc(sizeof(Match));
    match_init(match);
    match->dock = M_DOCK_BOTTOM;
    match->insert_where = M_BELOW;
    TAILQ_INSERT_TAIL(&(bottomdock->swallow_head), match, matches);

    FREE(bottomdock->name);
    bottomdock->name = sstrdup("bottomdock");

    sasprintf(&name, "[i3 con] bottom dockarea %s", con->name);
    x_set_name(bottomdock, name);
    FREE(name);
    DLOG("attaching\n");
    con_attach(bottomdock, con, false);
}

/*
 * Initializes at least one workspace for this output, trying the following
 * steps until there is at least one workspace:
 *
 * • Move existing workspaces, which are assigned to be on the given output, to
 *   the output.
 * • Create the first assigned workspace for this output.
 * • Create the first unused workspace.
 *
 */
void init_ws_for_output(Output *output, Con *content) {
    /* go through all assignments and move the existing workspaces to this output */
    struct Workspace_Assignment *assignment;
    TAILQ_FOREACH(assignment, &ws_assignments, ws_assignments) {
        if (strcmp(assignment->output, output->name) != 0)
            continue;

        /* check if this workspace actually exists */
        Con *workspace = NULL, *out;
        TAILQ_FOREACH(out, &(croot->nodes_head), nodes)
        GREP_FIRST(workspace, output_get_content(out),
                   !strcasecmp(child->name, assignment->name));
        if (workspace == NULL)
            continue;

        /* check that this workspace is not already attached (that means the
         * user configured this assignment twice) */
        Con *workspace_out = con_get_output(workspace);
        if (workspace_out == output->con) {
            LOG("Workspace \"%s\" assigned to output \"%s\", but it is already "
                "there. Do you have two assignment directives for the same "
                "workspace in your configuration file?\n",
                workspace->name, output->name);
            continue;
        }

        /* if so, move it over */
        LOG("Moving workspace \"%s\" from output \"%s\" to \"%s\" due to assignment\n",
            workspace->name, workspace_out->name, output->name);

        /* if the workspace is currently visible on that output, we need to
         * switch to a different workspace - otherwise the output would end up
         * with no active workspace */
        bool visible = workspace_is_visible(workspace);
        Con *previous = NULL;
        if (visible && (previous = TAILQ_NEXT(workspace, focused))) {
            LOG("Switching to previously used workspace \"%s\" on output \"%s\"\n",
                previous->name, workspace_out->name);
            workspace_show(previous);
        }

        /* Render the output on which the workspace was to get correct Rects.
         * Then, we need to work with the "content" container, since we cannot
         * be sure that the workspace itself was rendered at all (in case it’s
         * invisible, it won’t be rendered). */
        render_con(workspace_out, false);
        Con *ws_out_content = output_get_content(workspace_out);

        Con *floating_con;
        TAILQ_FOREACH(floating_con, &(workspace->floating_head), floating_windows)
        /* NB: We use output->con here because content is not yet rendered,
             * so it has a rect of {0, 0, 0, 0}. */
        floating_fix_coordinates(floating_con, &(ws_out_content->rect), &(output->con->rect));

        con_detach(workspace);
        con_attach(workspace, content, false);

        /* In case the workspace we just moved was visible but there was no
         * other workspace to switch to, we need to initialize the source
         * output aswell */
        if (visible && previous == NULL) {
            LOG("There is no workspace left on \"%s\", re-initializing\n",
                workspace_out->name);
            init_ws_for_output(get_output_by_name(workspace_out->name),
                               output_get_content(workspace_out));
            DLOG("Done re-initializing, continuing with \"%s\"\n", output->name);
        }
    }

    /* if a workspace exists, we are done now */
    if (!TAILQ_EMPTY(&(content->nodes_head))) {
        /* ensure that one of the workspaces is actually visible (in fullscreen
         * mode), if they were invisible before, this might not be the case. */
        Con *visible = NULL;
        GREP_FIRST(visible, content, child->fullscreen_mode == CF_OUTPUT);
        if (!visible) {
            visible = TAILQ_FIRST(&(content->nodes_head));
            focused = content;
            workspace_show(visible);
        }
        return;
    }

    /* otherwise, we create the first assigned ws for this output */
    TAILQ_FOREACH(assignment, &ws_assignments, ws_assignments) {
        if (strcmp(assignment->output, output->name) != 0)
            continue;

        LOG("Initializing first assigned workspace \"%s\" for output \"%s\"\n",
            assignment->name, assignment->output);
        focused = content;
        workspace_show_by_name(assignment->name);
        return;
    }

    /* if there is still no workspace, we create the first free workspace */
    DLOG("Now adding a workspace\n");
    Con *ws = create_workspace_on_output(output, content);

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
    DLOG("Output mode changed, updating rect\n");
    assert(output->con != NULL);
    output->con->rect = output->rect;

    Con *content, *workspace, *child;

    /* Point content to the container of the workspaces */
    content = output_get_content(output->con);

    /* Fix the position of all floating windows on this output.
     * The 'rect' of each workspace will be updated in src/render.c. */
    TAILQ_FOREACH(workspace, &(content->nodes_head), nodes) {
        TAILQ_FOREACH(child, &(workspace->floating_head), floating_windows) {
            floating_fix_coordinates(child, &(workspace->rect), &(output->con->rect));
        }
    }

    /* If default_orientation is NO_ORIENTATION, we change the orientation of
     * the workspaces and their childs depending on output resolution. This is
     * only done for workspaces with maximum one child. */
    if (config.default_orientation == NO_ORIENTATION) {
        TAILQ_FOREACH(workspace, &(content->nodes_head), nodes) {
            /* Workspaces with more than one child are left untouched because
             * we do not want to change an existing layout. */
            if (con_num_children(workspace) > 1)
                continue;

            workspace->layout = (output->rect.height > output->rect.width) ? L_SPLITV : L_SPLITH;
            DLOG("Setting workspace [%d,%s]'s layout to %d.\n", workspace->num, workspace->name, workspace->layout);
            if ((child = TAILQ_FIRST(&(workspace->nodes_head)))) {
                if (child->layout == L_SPLITV || child->layout == L_SPLITH)
                    child->layout = workspace->layout;
                DLOG("Setting child [%d,%s]'s layout to %d.\n", child->num, child->name, child->layout);
            }
        }
    }
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
    sasprintf(&new->name, "%.*s",
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
            else
                TAILQ_INSERT_TAIL(&outputs, new, outputs);
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
            else
                TAILQ_INSERT_TAIL(&outputs, new, outputs);
        }
        return;
    }

    new->changed = true;
}

/*
 * (Re-)queries the outputs via RandR and stores them in the list of outputs.
 *
 */
void randr_query_outputs(void) {
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
    else
        DLOG("primary output is %08x\n", primary->output);
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

    /* Ensure that all outputs which are active also have a con. This is
     * necessary because in the next step, a clone might get disabled. Example:
     * LVDS1 active, VGA1 gets activated as a clone of LVDS1 (has no con).
     * LVDS1 gets disabled. */
    TAILQ_FOREACH(output, &outputs, outputs) {
        if (output->active && output->con == NULL) {
            DLOG("Need to initialize a Con for output %s\n", output->name);
            output_init_con(output);
            output->changed = false;
        }
    }

    /* Handle outputs which have a new mode or are disabled now (either
     * because the user disabled them or because they are clones) */
    TAILQ_FOREACH(output, &outputs, outputs) {
        if (output->to_be_disabled) {
            output->active = false;
            DLOG("Output %s disabled, re-assigning workspaces/docks\n", output->name);

            first = get_first_output();

            /* TODO: refactor the following code into a nice function. maybe
             * use an on_destroy callback which is implement differently for
             * different container types (CT_CONTENT vs. CT_DOCKAREA)? */
            Con *first_content = output_get_content(first->con);

            if (output->con != NULL) {
                /* We need to move the workspaces from the disappearing output to the first output */
                /* 1: Get the con to focus next, if the disappearing ws is focused */
                Con *next = NULL;
                if (TAILQ_FIRST(&(croot->focus_head)) == output->con) {
                    DLOG("This output (%p) was focused! Getting next\n", output->con);
                    next = focused;
                    DLOG("next = %p\n", next);
                }

                /* 2: iterate through workspaces and re-assign them, fixing the coordinates
                 * of floating containers as we go */
                Con *current;
                Con *old_content = output_get_content(output->con);
                while (!TAILQ_EMPTY(&(old_content->nodes_head))) {
                    current = TAILQ_FIRST(&(old_content->nodes_head));
                    if (current != next && TAILQ_EMPTY(&(current->focus_head))) {
                        /* the workspace is empty and not focused, get rid of it */
                        DLOG("Getting rid of current = %p / %s (empty, unfocused)\n", current, current->name);
                        tree_close(current, DONT_KILL_WINDOW, false, false);
                        continue;
                    }
                    DLOG("Detaching current = %p / %s\n", current, current->name);
                    con_detach(current);
                    DLOG("Re-attaching current = %p / %s\n", current, current->name);
                    con_attach(current, first_content, false);
                    DLOG("Fixing the coordinates of floating containers\n");
                    Con *floating_con;
                    TAILQ_FOREACH(floating_con, &(current->floating_head), floating_windows)
                    floating_fix_coordinates(floating_con, &(output->con->rect), &(first->con->rect));
                    DLOG("Done, next\n");
                }
                DLOG("re-attached all workspaces\n");

                if (next) {
                    DLOG("now focusing next = %p\n", next);
                    con_focus(next);
                    workspace_show(con_get_workspace(next));
                }

                /* 3: move the dock clients to the first output */
                Con *child;
                TAILQ_FOREACH(child, &(output->con->nodes_head), nodes) {
                    if (child->type != CT_DOCKAREA)
                        continue;
                    DLOG("Handling dock con %p\n", child);
                    Con *dock;
                    while (!TAILQ_EMPTY(&(child->nodes_head))) {
                        dock = TAILQ_FIRST(&(child->nodes_head));
                        Con *nc;
                        Match *match;
                        nc = con_for_window(first->con, dock->window, &match);
                        DLOG("Moving dock client %p to nc %p\n", dock, nc);
                        con_detach(dock);
                        DLOG("Re-attaching\n");
                        con_attach(dock, nc, false);
                        DLOG("Done\n");
                    }
                }

                DLOG("destroying disappearing con %p\n", output->con);
                tree_close(output->con, DONT_KILL_WINDOW, true, false);
                DLOG("Done. Should be fine now\n");
                output->con = NULL;
            }

            output->to_be_disabled = false;
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

    /* Verifies that there is at least one active output as a side-effect. */
    get_first_output();

    /* Just go through each active output and assign one workspace */
    TAILQ_FOREACH(output, &outputs, outputs) {
        if (!output->active)
            continue;
        Con *content = output_get_content(output->con);
        if (!TAILQ_EMPTY(&(content->nodes_head)))
            continue;
        DLOG("Should add ws for output %s\n", output->name);
        init_ws_for_output(output, content);
    }

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
    if (!extreply->present) {
        disable_randr(conn);
        return;
    } else
        randr_query_outputs();

    if (event_base != NULL)
        *event_base = extreply->first_event;

    xcb_randr_select_input(conn, root,
                           XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE |
                               XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE |
                               XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE |
                               XCB_RANDR_NOTIFY_MASK_OUTPUT_PROPERTY);

    xcb_flush(conn);
}
