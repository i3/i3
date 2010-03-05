/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * For more information on RandR, please see the X.org RandR specification at
 * http://cgit.freedesktop.org/xorg/proto/randrproto/tree/randrproto.txt
 * (take your time to read it completely, it answers all questions).
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include <xcb/xcb.h>
#include <xcb/randr.h>

#include "queue.h"
#include "i3.h"
#include "data.h"
#include "table.h"
#include "util.h"
#include "layout.h"
#include "xcb.h"
#include "config.h"
#include "workspace.h"
#include "log.h"
#include "ewmh.h"

/* While a clean namespace is usually a pretty good thing, we really need
 * to use shorter names than the whole xcb_randr_* default names. */
typedef xcb_randr_get_crtc_info_reply_t crtc_info;
typedef xcb_randr_mode_info_t mode_info;
typedef xcb_randr_get_screen_resources_current_reply_t resources_reply;

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

static void initialize_output(xcb_connection_t *conn, Output *output,
                              Workspace *workspace) {
        i3Font *font = load_font(conn, config.font);

        workspace->output = output;
        output->current_workspace = workspace;

        /* Copy rect for the workspace */
        memcpy(&(workspace->rect), &(output->rect), sizeof(Rect));

        /* Map clients on the workspace, if any */
        workspace_map_clients(conn, workspace);

        /* Create a xoutput for each output */
        Rect bar_rect = {output->rect.x,
                         output->rect.y + output->rect.height - (font->height + 6),
                         output->rect.x + output->rect.width,
                         font->height + 6};
        uint32_t mask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
        uint32_t values[] = {1, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS};
        output->bar = create_window(conn, bar_rect, XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_CURSOR_LEFT_PTR, true, mask, values);
        output->bargc = xcb_generate_id(conn);
        xcb_create_gc(conn, output->bargc, output->bar, 0, 0);

        SLIST_INIT(&(output->dock_clients));

        DLOG("initialized output at (%d, %d) with %d x %d\n",
                        output->rect.x, output->rect.y, output->rect.width, output->rect.height);
}

/*
 * Disables RandR support by creating exactly one output with the size of the
 * X11 screen.
 *
 */
static void disable_randr(xcb_connection_t *conn) {
        xcb_screen_t *root_screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

        DLOG("RandR extension unusable, disabling.\n");

        Output *s = scalloc(sizeof(Output));

        s->active = true;
        s->rect.x = 0;
        s->rect.y = 0;
        s->rect.width = root_screen->width_in_pixels;
        s->rect.height = root_screen->height_in_pixels;

        TAILQ_INSERT_TAIL(&outputs, s, outputs);

        randr_disabled = true;
}

/*
 * Searches for a mode in the current RandR configuration by the mode id.
 * Returns NULL if no such mode could be found (should never happen).
 *
 */
static mode_info *get_mode_by_id(resources_reply *reply, xcb_randr_mode_t mode) {
        xcb_randr_mode_info_iterator_t it;

        for (it = xcb_randr_get_screen_resources_current_modes_iterator(reply);
                it.rem > 0;
                xcb_randr_mode_info_next(&it)) {
                if (it.data->id == mode)
                        return it.data;
        }

        return NULL;
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
        i3Font *font = load_font(conn, config.font);
        Workspace *ws;
        Client *client;

        DLOG("Output mode changed, reconfiguring bar, updating workspaces\n");
        Rect bar_rect = {output->rect.x,
                         output->rect.y + output->rect.height - (font->height + 6),
                         output->rect.x + output->rect.width,
                         font->height + 6};

        xcb_set_window_rect(conn, output->bar, bar_rect);

        /* go through all workspaces and set force_reconfigure */
        TAILQ_FOREACH(ws, workspaces, workspaces) {
                if (ws->output != output)
                        continue;

                /* Update dimensions from output */
                memcpy(&(ws->rect), &(ws->output->rect), sizeof(Rect));

                SLIST_FOREACH(client, &(ws->focus_stack), focus_clients)
                        client->force_reconfigure = true;

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

        /* the CRTC runs in a specific mode, while the position is stored in
         * the output itself */
        mode_info *mode;

        Output *new = get_output_by_id(id);
        bool existing = (new != NULL);
        if (!existing)
                new = scalloc(sizeof(Output));
        new->id = id;
        asprintf(&new->name, "%.*s",
                        xcb_randr_get_output_info_name_length(output),
                        xcb_randr_get_output_info_name(output));

        DLOG("found output with name %s\n", new->name);

        /* Even if no CRTC is used at the moment, we store the output so that
         * we do not need to change the list ever again (we only update the
         * position/size) */
        if (output->crtc == XCB_NONE) {
                if (!existing)
                        TAILQ_INSERT_TAIL(&outputs, new, outputs);
                else if (new->active) {
                        new->to_be_disabled = true;

                }
                free(output);
                return;
        }

        xcb_randr_get_crtc_info_cookie_t icookie;
        icookie = xcb_randr_get_crtc_info(conn, output->crtc, cts);
        if ((crtc = xcb_randr_get_crtc_info_reply(conn, icookie, NULL)) == NULL ||
            (mode = get_mode_by_id(res, crtc->mode)) == NULL) {
                DLOG("Skipping output %s: could not get CRTC/mode (%p/%p)\n",
                     new->name, crtc, mode);
                free(new);
                free(output);
                return;
        }

        new->active = true;
        bool updated = update_if_necessary(&(new->rect.x), crtc->x) |
                       update_if_necessary(&(new->rect.y), crtc->y) |
                       update_if_necessary(&(new->rect.width), mode->width) |
                       update_if_necessary(&(new->rect.height), mode->height);

        DLOG("mode: %dx%d+%d+%d\n", new->rect.width, new->rect.height,
                                    new->rect.x, new->rect.y);

        /* If we don’t need to change an existing output or if the output
         * does not exist in the first place, the case is simple: we either
         * need to insert the new output or we are done. */
        if (!updated || !existing) {
                if (!existing)
                        TAILQ_INSERT_TAIL(&outputs, new, outputs);
                free(output);
                return;
        }

        new->changed = true;
}

/*
 * (Re-)queries the outputs via RandR and stores them in the list of outputs.
 *
 */
void randr_query_outputs(xcb_connection_t *conn) {
        Workspace *ws;
        Output *output, *other, *first;
        xcb_randr_get_screen_resources_current_cookie_t rcookie;
        resources_reply *res;
        /* timestamp of the configuration so that we get consistent replies to all
         * requests (if the configuration changes between our different calls) */
        xcb_timestamp_t cts;

        /* an output is VGA-1, LVDS-1, etc. (usually physical video outputs) */
        xcb_randr_output_t *randr_outputs;

        if (randr_disabled)
                return;

        /* Get screen resources (crtcs, outputs, modes) */
        rcookie = xcb_randr_get_screen_resources_current(conn, root);
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
        }

        free(res);
        /* Check for clones, disable the clones and reduce the mode to the
         * lowest common mode */
        TAILQ_FOREACH(output, &outputs, outputs) {
                if (!output->active || output->to_be_disabled)
                        continue;
                DLOG("output %p, position (%d, %d), checking for clones\n",
                        output, output->rect.x, output->rect.y);

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

                        bool needs_init = (first->current_workspace == NULL);

                        TAILQ_FOREACH(ws, workspaces, workspaces) {
                                if (ws->output != output)
                                        continue;

                                workspace_assign_to(ws, first, true);
                                if (!needs_init)
                                        continue;
                                initialize_output(conn, first, ws);
                                needs_init = false;
                        }

                        Client *dock;
                        while (!SLIST_EMPTY(&(output->dock_clients))) {
                                dock = SLIST_FIRST(&(output->dock_clients));
                                SLIST_INSERT_HEAD(&(first->dock_clients), dock, dock_clients);
                                SLIST_REMOVE_HEAD(&(output->dock_clients), dock_clients);
                        }
                        output->current_workspace = NULL;
                        output->to_be_disabled = false;
                } else if (output->changed) {
                        output_change_mode(conn, output);
                        output->changed = false;
                }
        }

        ewmh_update_workarea();

        /* Just go through each active output and associate one workspace */
        TAILQ_FOREACH(output, &outputs, outputs) {
                if (!output->active || output->current_workspace != NULL)
                        continue;
                ws = get_first_workspace_for_output(output);
                initialize_output(conn, output, ws);
        }

        /* render_layout flushes */
        render_layout(conn);
}

/*
 * We have just established a connection to the X server and need the initial
 * XRandR information to setup workspaces for each screen.
 *
 */
void initialize_randr(xcb_connection_t *conn, int *event_base) {
        const xcb_query_extension_reply_t *extreply;

        extreply = xcb_get_extension_data(conn, &xcb_randr_id);
        if (!extreply->present)
                disable_randr(conn);
        else randr_query_outputs(conn);

        if (event_base != NULL)
                *event_base = extreply->first_event;

        xcb_randr_select_input(conn, root,
                XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE |
                XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE |
                XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE |
                XCB_RANDR_NOTIFY_MASK_OUTPUT_PROPERTY);

        xcb_flush(conn);
}
