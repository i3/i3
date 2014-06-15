#undef I3__FILE__
#define I3__FILE__ "workspace.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * workspace.c: Modifying workspaces, accessing them, moving containers to
 *              workspaces.
 *
 */
#include "all.h"

/* Stores a copy of the name of the last used workspace for the workspace
 * back-and-forth switching. */
static char *previous_workspace_name = NULL;

/*
 * Sets ws->layout to splith/splitv if default_orientation was specified in the
 * configfile. Otherwise, it uses splith/splitv depending on whether the output
 * is higher than wide.
 *
 */
static void _workspace_apply_default_orientation(Con *ws) {
    /* If default_orientation is set to NO_ORIENTATION we determine
     * orientation depending on output resolution. */
    if (config.default_orientation == NO_ORIENTATION) {
        Con *output = con_get_output(ws);
        ws->layout = (output->rect.height > output->rect.width) ? L_SPLITV : L_SPLITH;
        DLOG("Auto orientation. Workspace size set to (%d,%d), setting layout to %d.\n",
             output->rect.width, output->rect.height, ws->layout);
    } else {
        ws->layout = (config.default_orientation == HORIZ) ? L_SPLITH : L_SPLITV;
    }
}

/*
 * Returns a pointer to the workspace with the given number (starting at 0),
 * creating the workspace if necessary (by allocating the necessary amount of
 * memory and initializing the data structures correctly).
 *
 */
Con *workspace_get(const char *num, bool *created) {
    Con *output, *workspace = NULL;

    TAILQ_FOREACH (output, &(croot->nodes_head), nodes)
        GREP_FIRST(workspace, output_get_content(output), !strcasecmp(child->name, num));

    if (workspace == NULL) {
        LOG("Creating new workspace \"%s\"\n", num);
        /* unless an assignment is found, we will create this workspace on the current output */
        output = con_get_output(focused);
        /* look for assignments */
        struct Workspace_Assignment *assignment;

        /* We set workspace->num to the number if this workspace’s name begins
         * with a positive number. Otherwise it’s a named ws and num will be
         * -1. */
        long parsed_num = ws_name_to_number(num);

        TAILQ_FOREACH (assignment, &ws_assignments, ws_assignments) {
            if (strcmp(assignment->name, num) == 0) {
                DLOG("Found workspace name assignment to output \"%s\"\n", assignment->output);
                GREP_FIRST(output, croot, !strcmp(child->name, assignment->output));
                break;
            } else if (parsed_num != -1 && name_is_digits(assignment->name) && ws_name_to_number(assignment->name) == parsed_num) {
                DLOG("Found workspace number assignment to output \"%s\"\n", assignment->output);
                GREP_FIRST(output, croot, !strcmp(child->name, assignment->output));
            }
        }

        Con *content = output_get_content(output);
        LOG("got output %p with content %p\n", output, content);
        /* We need to attach this container after setting its type. con_attach
         * will handle CT_WORKSPACEs differently */
        workspace = con_new(NULL, NULL);
        char *name;
        sasprintf(&name, "[i3 con] workspace %s", num);
        x_set_name(workspace, name);
        free(name);
        workspace->type = CT_WORKSPACE;
        FREE(workspace->name);
        workspace->name = sstrdup(num);
        workspace->workspace_layout = config.default_layout;
        workspace->num = parsed_num;
        LOG("num = %d\n", workspace->num);

        workspace->parent = content;
        _workspace_apply_default_orientation(workspace);

        con_attach(workspace, content, false);

        ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"init\"}");
        if (created != NULL)
            *created = true;
    } else if (created != NULL) {
        *created = false;
    }

    return workspace;
}

/*
 * Returns a pointer to a new workspace in the given output. The workspace
 * is created attached to the tree hierarchy through the given content
 * container.
 *
 */
Con *create_workspace_on_output(Output *output, Con *content) {
    /* add a workspace to this output */
    Con *out, *current;
    char *name;
    bool exists = true;
    Con *ws = con_new(NULL, NULL);
    ws->type = CT_WORKSPACE;

    /* try the configured workspace bindings first to find a free name */
    Binding *bind;
    TAILQ_FOREACH (bind, bindings, bindings) {
        DLOG("binding with command %s\n", bind->command);
        if (strlen(bind->command) < strlen("workspace ") ||
            strncasecmp(bind->command, "workspace", strlen("workspace")) != 0)
            continue;
        DLOG("relevant command = %s\n", bind->command);
        char *target = bind->command + strlen("workspace ");
        while ((*target == ' ' || *target == '\t') && target != '\0')
            target++;
        /* We check if this is the workspace
         * next/prev/next_on_output/prev_on_output/back_and_forth/number command.
         * Beware: The workspace names "next", "prev", "next_on_output",
         * "prev_on_output", "number", "back_and_forth" and "current" are OK,
         * so we check before stripping the double quotes */
        if (strncasecmp(target, "next", strlen("next")) == 0 ||
            strncasecmp(target, "prev", strlen("prev")) == 0 ||
            strncasecmp(target, "next_on_output", strlen("next_on_output")) == 0 ||
            strncasecmp(target, "prev_on_output", strlen("prev_on_output")) == 0 ||
            strncasecmp(target, "number", strlen("number")) == 0 ||
            strncasecmp(target, "back_and_forth", strlen("back_and_forth")) == 0 ||
            strncasecmp(target, "current", strlen("current")) == 0)
            continue;
        if (*target == '"')
            target++;
        if (strncasecmp(target, "__", strlen("__")) == 0) {
            LOG("Cannot create workspace \"%s\". Names starting with __ are i3-internal.\n", target);
            continue;
        }
        FREE(ws->name);
        ws->name = strdup(target);
        if (ws->name[strlen(ws->name) - 1] == '"')
            ws->name[strlen(ws->name) - 1] = '\0';
        DLOG("trying name *%s*\n", ws->name);

        /* Ensure that this workspace is not assigned to a different output —
         * otherwise we would create it, then move it over to its output, then
         * find a new workspace, etc… */
        bool assigned = false;
        struct Workspace_Assignment *assignment;
        TAILQ_FOREACH (assignment, &ws_assignments, ws_assignments) {
            if (strcmp(assignment->name, ws->name) != 0 ||
                strcmp(assignment->output, output->name) == 0)
                continue;

            assigned = true;
            break;
        }

        if (assigned)
            continue;

        current = NULL;
        TAILQ_FOREACH (out, &(croot->nodes_head), nodes)
            GREP_FIRST(current, output_get_content(out), !strcasecmp(child->name, ws->name));

        exists = (current != NULL);
        if (!exists) {
            /* Set ->num to the number of the workspace, if the name actually
             * is a number or starts with a number */
            char *endptr = NULL;
            long parsed_num = strtol(ws->name, &endptr, 10);
            if (parsed_num == LONG_MIN ||
                parsed_num == LONG_MAX ||
                parsed_num < 0 ||
                endptr == ws->name)
                ws->num = -1;
            else
                ws->num = parsed_num;
            LOG("Used number %d for workspace with name %s\n", ws->num, ws->name);

            break;
        }
    }

    if (exists) {
        /* get the next unused workspace number */
        DLOG("Getting next unused workspace by number\n");
        int c = 0;
        while (exists) {
            c++;

            ws->num = c;

            current = NULL;
            TAILQ_FOREACH (out, &(croot->nodes_head), nodes)
                GREP_FIRST(current, output_get_content(out), child->num == ws->num);
            exists = (current != NULL);

            DLOG("result for ws %d: exists = %d\n", c, exists);
        }
        sasprintf(&(ws->name), "%d", c);
    }
    con_attach(ws, content, false);

    sasprintf(&name, "[i3 con] workspace %s", ws->name);
    x_set_name(ws, name);
    free(name);

    ws->fullscreen_mode = CF_OUTPUT;

    ws->workspace_layout = config.default_layout;
    _workspace_apply_default_orientation(ws);

    return ws;
}

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
    Con *fs = con_get_fullscreen_con(output, CF_OUTPUT);
    LOG("workspace visible? fs = %p, ws = %p\n", fs, ws);
    return (fs == ws);
}

/*
 * XXX: we need to clean up all this recursive walking code.
 *
 */
Con *_get_sticky(Con *con, const char *sticky_group, Con *exclude) {
    Con *current;

    TAILQ_FOREACH (current, &(con->nodes_head), nodes) {
        if (current != exclude &&
            current->sticky_group != NULL &&
            current->window != NULL &&
            strcmp(current->sticky_group, sticky_group) == 0)
            return current;

        Con *recurse = _get_sticky(current, sticky_group, exclude);
        if (recurse != NULL)
            return recurse;
    }

    TAILQ_FOREACH (current, &(con->floating_head), floating_windows) {
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
    TAILQ_FOREACH (current, &(con->nodes_head), nodes) {
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

    TAILQ_FOREACH (current, &(con->floating_head), floating_windows)
        workspace_reassign_sticky(current);
}

/*
 * Callback to reset the urgent flag of the given con to false. May be started by
 * _workspace_show to avoid urgency hints being lost by switching to a workspace
 * focusing the con.
 *
 */
static void workspace_defer_update_urgent_hint_cb(EV_P_ ev_timer *w, int revents) {
    Con *con = w->data;

    DLOG("Resetting urgency flag of con %p by timer\n", con);
    con->urgent = false;
    con_update_parents_urgency(con);
    workspace_update_urgent_flag(con_get_workspace(con));
    tree_render();

    ev_timer_stop(main_loop, con->urgency_timer);
    FREE(con->urgency_timer);
}

static void _workspace_show(Con *workspace) {
    Con *current, *old = NULL;

    /* safe-guard against showing i3-internal workspaces like __i3_scratch */
    if (con_is_internal(workspace))
        return;

    /* disable fullscreen for the other workspaces and get the workspace we are
     * currently on. */
    TAILQ_FOREACH (current, &(workspace->parent->nodes_head), nodes) {
        if (current->fullscreen_mode == CF_OUTPUT)
            old = current;
        current->fullscreen_mode = CF_NONE;
    }

    /* enable fullscreen for the target workspace. If it happens to be the
     * same one we are currently on anyways, we can stop here. */
    workspace->fullscreen_mode = CF_OUTPUT;
    current = con_get_workspace(focused);
    if (workspace == current) {
        DLOG("Not switching, already there.\n");
        return;
    }

    /* Remember currently focused workspace for switching back to it later with
     * the 'workspace back_and_forth' command.
     * NOTE: We have to duplicate the name as the original will be freed when
     * the corresponding workspace is cleaned up.
     * NOTE: Internal cons such as __i3_scratch (when a scratchpad window is
     * focused) are skipped, see bug #868. */
    if (current && !con_is_internal(current)) {
        FREE(previous_workspace_name);
        if (current) {
            previous_workspace_name = sstrdup(current->name);
            DLOG("Setting previous_workspace_name = %s\n", previous_workspace_name);
        }
    }

    workspace_reassign_sticky(workspace);

    DLOG("switching to %p / %s\n", workspace, workspace->name);
    Con *next = con_descend_focused(workspace);

    /* Memorize current output */
    Con *old_output = con_get_output(focused);

    /* Display urgency hint for a while if the newly visible workspace would
     * focus and thereby immediately destroy it */
    if (next->urgent && (int)(config.workspace_urgency_timer * 1000) > 0) {
        /* focus for now… */
        con_focus(next);

        /* … but immediately reset urgency flags; they will be set to false by
         * the timer callback in case the container is focused at the time of
         * its expiration */
        focused->urgent = true;
        workspace->urgent = true;

        if (focused->urgency_timer == NULL) {
            DLOG("Deferring reset of urgency flag of con %p on newly shown workspace %p\n",
                 focused, workspace);
            focused->urgency_timer = scalloc(sizeof(struct ev_timer));
            /* use a repeating timer to allow for easy resets */
            ev_timer_init(focused->urgency_timer, workspace_defer_update_urgent_hint_cb,
                          config.workspace_urgency_timer, config.workspace_urgency_timer);
            focused->urgency_timer->data = focused;
            ev_timer_start(main_loop, focused->urgency_timer);
        } else {
            DLOG("Resetting urgency timer of con %p on workspace %p\n",
                 focused, workspace);
            ev_timer_again(main_loop, focused->urgency_timer);
        }
    } else
        con_focus(next);

    ipc_send_workspace_focus_event(workspace, current);

    DLOG("old = %p / %s\n", old, (old ? old->name : "(null)"));
    /* Close old workspace if necessary. This must be done *after* doing
     * urgency handling, because tree_close() will do a con_focus() on the next
     * client, which will clear the urgency flag too early. Also, there is no
     * way for con_focus() to know about when to clear urgency immediately and
     * when to defer it. */
    if (old && TAILQ_EMPTY(&(old->nodes_head)) && TAILQ_EMPTY(&(old->floating_head))) {
        /* check if this workspace is currently visible */
        if (!workspace_is_visible(old)) {
            LOG("Closing old workspace (%p / %s), it is empty\n", old, old->name);
            tree_close(old, DONT_KILL_WINDOW, false, false);
            ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"empty\"}");
        }
    }

    workspace->fullscreen_mode = CF_OUTPUT;
    LOG("focused now = %p / %s\n", focused, focused->name);

    /* Set mouse pointer */
    Con *new_output = con_get_output(focused);
    if (old_output != new_output) {
        x_set_warp_to(&next->rect);
    }

    /* Update the EWMH hints */
    ewmh_update_current_desktop();
}

/*
 * Switches to the given workspace
 *
 */
void workspace_show(Con *workspace) {
    _workspace_show(workspace);
}

/*
 * Looks up the workspace by name and switches to it.
 *
 */
void workspace_show_by_name(const char *num) {
    Con *workspace;
    workspace = workspace_get(num, NULL);
    _workspace_show(workspace);
}

/*
 * Focuses the next workspace.
 *
 */
Con *workspace_next(void) {
    Con *current = con_get_workspace(focused);
    Con *next = NULL;
    Con *output;

    if (current->num == -1) {
        /* If currently a named workspace, find next named workspace. */
        next = TAILQ_NEXT(current, nodes);
    } else {
        /* If currently a numbered workspace, find next numbered workspace. */
        TAILQ_FOREACH (output, &(croot->nodes_head), nodes) {
            /* Skip outputs starting with __, they are internal. */
            if (con_is_internal(output))
                continue;
            NODES_FOREACH (output_get_content(output)) {
                if (child->type != CT_WORKSPACE)
                    continue;
                if (child->num == -1)
                    break;
                /* Need to check child against current and next because we are
                 * traversing multiple lists and thus are not guaranteed the
                 * relative order between the list of workspaces. */
                if (current->num < child->num && (!next || child->num < next->num))
                    next = child;
            }
        }
    }

    /* Find next named workspace. */
    if (!next) {
        bool found_current = false;
        TAILQ_FOREACH (output, &(croot->nodes_head), nodes) {
            /* Skip outputs starting with __, they are internal. */
            if (con_is_internal(output))
                continue;
            NODES_FOREACH (output_get_content(output)) {
                if (child->type != CT_WORKSPACE)
                    continue;
                if (child == current) {
                    found_current = 1;
                } else if (child->num == -1 && (current->num != -1 || found_current)) {
                    next = child;
                    goto workspace_next_end;
                }
            }
        }
    }

    /* Find first workspace. */
    if (!next) {
        TAILQ_FOREACH (output, &(croot->nodes_head), nodes) {
            /* Skip outputs starting with __, they are internal. */
            if (con_is_internal(output))
                continue;
            NODES_FOREACH (output_get_content(output)) {
                if (child->type != CT_WORKSPACE)
                    continue;
                if (!next || (child->num != -1 && child->num < next->num))
                    next = child;
            }
        }
    }
workspace_next_end:
    return next;
}

/*
 * Focuses the previous workspace.
 *
 */
Con *workspace_prev(void) {
    Con *current = con_get_workspace(focused);
    Con *prev = NULL;
    Con *output;

    if (current->num == -1) {
        /* If named workspace, find previous named workspace. */
        prev = TAILQ_PREV(current, nodes_head, nodes);
        if (prev && prev->num != -1)
            prev = NULL;
    } else {
        /* If numbered workspace, find previous numbered workspace. */
        TAILQ_FOREACH_REVERSE (output, &(croot->nodes_head), nodes_head, nodes) {
            /* Skip outputs starting with __, they are internal. */
            if (con_is_internal(output))
                continue;
            NODES_FOREACH_REVERSE (output_get_content(output)) {
                if (child->type != CT_WORKSPACE || child->num == -1)
                    continue;
                /* Need to check child against current and previous because we
                 * are traversing multiple lists and thus are not guaranteed
                 * the relative order between the list of workspaces. */
                if (current->num > child->num && (!prev || child->num > prev->num))
                    prev = child;
            }
        }
    }

    /* Find previous named workspace. */
    if (!prev) {
        bool found_current = false;
        TAILQ_FOREACH_REVERSE (output, &(croot->nodes_head), nodes_head, nodes) {
            /* Skip outputs starting with __, they are internal. */
            if (con_is_internal(output))
                continue;
            NODES_FOREACH_REVERSE (output_get_content(output)) {
                if (child->type != CT_WORKSPACE)
                    continue;
                if (child == current) {
                    found_current = true;
                } else if (child->num == -1 && (current->num != -1 || found_current)) {
                    prev = child;
                    goto workspace_prev_end;
                }
            }
        }
    }

    /* Find last workspace. */
    if (!prev) {
        TAILQ_FOREACH_REVERSE (output, &(croot->nodes_head), nodes_head, nodes) {
            /* Skip outputs starting with __, they are internal. */
            if (con_is_internal(output))
                continue;
            NODES_FOREACH_REVERSE (output_get_content(output)) {
                if (child->type != CT_WORKSPACE)
                    continue;
                if (!prev || child->num > prev->num)
                    prev = child;
            }
        }
    }

workspace_prev_end:
    return prev;
}

/*
 * Focuses the next workspace on the same output.
 *
 */
Con *workspace_next_on_output(void) {
    Con *current = con_get_workspace(focused);
    Con *next = NULL;
    Con *output = con_get_output(focused);

    if (current->num == -1) {
        /* If currently a named workspace, find next named workspace. */
        next = TAILQ_NEXT(current, nodes);
    } else {
        /* If currently a numbered workspace, find next numbered workspace. */
        NODES_FOREACH (output_get_content(output)) {
            if (child->type != CT_WORKSPACE)
                continue;
            if (child->num == -1)
                break;
            /* Need to check child against current and next because we are
             * traversing multiple lists and thus are not guaranteed the
             * relative order between the list of workspaces. */
            if (current->num < child->num && (!next || child->num < next->num))
                next = child;
        }
    }

    /* Find next named workspace. */
    if (!next) {
        bool found_current = false;
        NODES_FOREACH (output_get_content(output)) {
            if (child->type != CT_WORKSPACE)
                continue;
            if (child == current) {
                found_current = 1;
            } else if (child->num == -1 && (current->num != -1 || found_current)) {
                next = child;
                goto workspace_next_on_output_end;
            }
        }
    }

    /* Find first workspace. */
    if (!next) {
        NODES_FOREACH (output_get_content(output)) {
            if (child->type != CT_WORKSPACE)
                continue;
            if (!next || (child->num != -1 && child->num < next->num))
                next = child;
        }
    }
workspace_next_on_output_end:
    return next;
}

/*
 * Focuses the previous workspace on same output.
 *
 */
Con *workspace_prev_on_output(void) {
    Con *current = con_get_workspace(focused);
    Con *prev = NULL;
    Con *output = con_get_output(focused);
    DLOG("output = %s\n", output->name);

    if (current->num == -1) {
        /* If named workspace, find previous named workspace. */
        prev = TAILQ_PREV(current, nodes_head, nodes);
        if (prev && prev->num != -1)
            prev = NULL;
    } else {
        /* If numbered workspace, find previous numbered workspace. */
        NODES_FOREACH_REVERSE (output_get_content(output)) {
            if (child->type != CT_WORKSPACE || child->num == -1)
                continue;
            /* Need to check child against current and previous because we
             * are traversing multiple lists and thus are not guaranteed
             * the relative order between the list of workspaces. */
            if (current->num > child->num && (!prev || child->num > prev->num))
                prev = child;
        }
    }

    /* Find previous named workspace. */
    if (!prev) {
        bool found_current = false;
        NODES_FOREACH_REVERSE (output_get_content(output)) {
            if (child->type != CT_WORKSPACE)
                continue;
            if (child == current) {
                found_current = true;
            } else if (child->num == -1 && (current->num != -1 || found_current)) {
                prev = child;
                goto workspace_prev_on_output_end;
            }
        }
    }

    /* Find last workspace. */
    if (!prev) {
        NODES_FOREACH_REVERSE (output_get_content(output)) {
            if (child->type != CT_WORKSPACE)
                continue;
            if (!prev || child->num > prev->num)
                prev = child;
        }
    }

workspace_prev_on_output_end:
    return prev;
}

/*
 * Focuses the previously focused workspace.
 *
 */
void workspace_back_and_forth(void) {
    if (!previous_workspace_name) {
        DLOG("No previous workspace name set. Not switching.");
        return;
    }

    workspace_show_by_name(previous_workspace_name);
}

/*
 * Returns the previously focused workspace con, or NULL if unavailable.
 *
 */
Con *workspace_back_and_forth_get(void) {
    if (!previous_workspace_name) {
        DLOG("no previous workspace name set.");
        return NULL;
    }

    Con *workspace;
    workspace = workspace_get(previous_workspace_name, NULL);

    return workspace;
}

static bool get_urgency_flag(Con *con) {
    Con *child;
    TAILQ_FOREACH (child, &(con->nodes_head), nodes)
        if (child->urgent || get_urgency_flag(child))
            return true;

    TAILQ_FOREACH (child, &(con->floating_head), floating_windows)
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

/*
 * 'Forces' workspace orientation by moving all cons into a new split-con with
 * the same layout as the workspace and then changing the workspace layout.
 *
 */
void ws_force_orientation(Con *ws, orientation_t orientation) {
    /* 1: create a new split container */
    Con *split = con_new(NULL, NULL);
    split->parent = ws;

    /* 2: copy layout from workspace */
    split->layout = ws->layout;

    Con *old_focused = TAILQ_FIRST(&(ws->focus_head));

    /* 3: move the existing cons of this workspace below the new con */
    DLOG("Moving cons\n");
    while (!TAILQ_EMPTY(&(ws->nodes_head))) {
        Con *child = TAILQ_FIRST(&(ws->nodes_head));
        con_detach(child);
        con_attach(child, split, true);
    }

    /* 4: switch workspace layout */
    ws->layout = (orientation == HORIZ) ? L_SPLITH : L_SPLITV;
    DLOG("split->layout = %d, ws->layout = %d\n", split->layout, ws->layout);

    /* 5: attach the new split container to the workspace */
    DLOG("Attaching new split (%p) to ws (%p)\n", split, ws);
    con_attach(split, ws, false);

    /* 6: fix the percentages */
    con_fix_percent(ws);

    if (old_focused)
        con_focus(old_focused);
}

/*
 * Called when a new con (with a window, not an empty or split con) should be
 * attached to the workspace (for example when managing a new window or when
 * moving an existing window to the workspace level).
 *
 * Depending on the workspace_layout setting, this function either returns the
 * workspace itself (default layout) or creates a new stacked/tabbed con and
 * returns that.
 *
 */
Con *workspace_attach_to(Con *ws) {
    DLOG("Attaching a window to workspace %p / %s\n", ws, ws->name);

    if (ws->workspace_layout == L_DEFAULT) {
        DLOG("Default layout, just attaching it to the workspace itself.\n");
        return ws;
    }

    DLOG("Non-default layout, creating a new split container\n");
    /* 1: create a new split container */
    Con *new = con_new(NULL, NULL);
    new->parent = ws;

    /* 2: set the requested layout on the split con */
    new->layout = ws->workspace_layout;

    /* 4: attach the new split container to the workspace */
    DLOG("Attaching new split %p to workspace %p\n", new, ws);
    con_attach(new, ws, false);

    return new;
}

/**
 * Creates a new container and re-parents all of children from the given
 * workspace into it.
 *
 * The container inherits the layout from the workspace.
 */
Con *workspace_encapsulate(Con *ws) {
    if (TAILQ_EMPTY(&(ws->nodes_head))) {
        ELOG("Workspace %p / %s has no children to encapsulate\n", ws, ws->name);
        return NULL;
    }

    Con *new = con_new(NULL, NULL);
    new->parent = ws;
    new->layout = ws->layout;

    DLOG("Moving children of workspace %p / %s into container %p\n",
         ws, ws->name, new);

    Con *child;
    while (!TAILQ_EMPTY(&(ws->nodes_head))) {
        child = TAILQ_FIRST(&(ws->nodes_head));
        con_detach(child);
        con_attach(child, new, true);
    }

    con_attach(new, ws, true);

    return new;
}
