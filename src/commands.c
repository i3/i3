/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * commands.c: all command functions (see commands_parser.c)
 *
 */
#include <float.h>

#include "all.h"
#include "cmdparse.tab.h"

/** When the command did not include match criteria (!), we use the currently
 * focused command. Do not confuse this case with a command which included
 * criteria but which did not match any windows. This macro has to be called in
 * every command.
 */
#define HANDLE_EMPTY_MATCH do { \
    if (match_is_empty(current_match)) { \
        owindow *ow = smalloc(sizeof(owindow)); \
        ow->con = focused; \
        TAILQ_INIT(&owindows); \
        TAILQ_INSERT_TAIL(&owindows, ow, owindows); \
    } \
} while (0)

static owindows_head owindows;

/*
 * Returns true if a is definitely greater than b (using the given epsilon)
 *
 */
static bool definitelyGreaterThan(float a, float b, float epsilon) {
    return (a - b) > ( (fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

static Output *get_output_from_string(Output *current_output, const char *output_str) {
    Output *output;

    if (strcasecmp(output_str, "left") == 0) {
        output = get_output_next(D_LEFT, current_output);
        if (!output)
            output = get_output_most(D_RIGHT, current_output);
    } else if (strcasecmp(output_str, "right") == 0) {
        output = get_output_next(D_RIGHT, current_output);
        if (!output)
            output = get_output_most(D_LEFT, current_output);
    } else if (strcasecmp(output_str, "up") == 0) {
        output = get_output_next(D_UP, current_output);
        if (!output)
            output = get_output_most(D_DOWN, current_output);
    } else if (strcasecmp(output_str, "down") == 0) {
        output = get_output_next(D_DOWN, current_output);
        if (!output)
            output = get_output_most(D_UP, current_output);
    } else output = get_output_by_name(output_str);

    return output;
}

char *cmd_criteria_init(Match *current_match) {
    DLOG("Initializing criteria, current_match = %p\n", current_match);
    match_init(current_match);
    TAILQ_INIT(&owindows);
    /* copy all_cons */
    Con *con;
    TAILQ_FOREACH(con, &all_cons, all_cons) {
        owindow *ow = smalloc(sizeof(owindow));
        ow->con = con;
        TAILQ_INSERT_TAIL(&owindows, ow, owindows);
    }

    /* This command is internal and does not generate a JSON reply. */
    return NULL;
}

char *cmd_criteria_match_windows(Match *current_match) {
    owindow *next, *current;

    DLOG("match specification finished, matching...\n");
    /* copy the old list head to iterate through it and start with a fresh
     * list which will contain only matching windows */
    struct owindows_head old = owindows;
    TAILQ_INIT(&owindows);
    for (next = TAILQ_FIRST(&old); next != TAILQ_END(&old);) {
        /* make a copy of the next pointer and advance the pointer to the
         * next element as we are going to invalidate the element’s
         * next/prev pointers by calling TAILQ_INSERT_TAIL later */
        current = next;
        next = TAILQ_NEXT(next, owindows);

        DLOG("checking if con %p / %s matches\n", current->con, current->con->name);
        if (current_match->con_id != NULL) {
            if (current_match->con_id == current->con) {
                DLOG("matches container!\n");
                TAILQ_INSERT_TAIL(&owindows, current, owindows);
            }
        } else if (current_match->mark != NULL && current->con->mark != NULL &&
                   regex_matches(current_match->mark, current->con->mark)) {
            DLOG("match by mark\n");
            TAILQ_INSERT_TAIL(&owindows, current, owindows);
        } else {
            if (current->con->window == NULL)
                continue;
            if (match_matches_window(current_match, current->con->window)) {
                DLOG("matches window!\n");
                TAILQ_INSERT_TAIL(&owindows, current, owindows);
            } else {
                DLOG("doesnt match\n");
                free(current);
            }
        }
    }

    TAILQ_FOREACH(current, &owindows, owindows) {
        DLOG("matching: %p / %s\n", current->con, current->con->name);
    }

    /* This command is internal and does not generate a JSON reply. */
    return NULL;
}

char *cmd_criteria_add(Match *current_match, char *ctype, char *cvalue) {
    DLOG("ctype=*%s*, cvalue=*%s*\n", ctype, cvalue);

    if (strcmp(ctype, "class") == 0) {
        current_match->class = regex_new(cvalue);
        return NULL;
    }

    if (strcmp(ctype, "instance") == 0) {
        current_match->instance = regex_new(cvalue);
        return NULL;
    }

    if (strcmp(ctype, "window_role") == 0) {
        current_match->role = regex_new(cvalue);
        return NULL;
    }

    if (strcmp(ctype, "con_id") == 0) {
        char *end;
        long parsed = strtol(cvalue, &end, 10);
        if (parsed == LONG_MIN ||
            parsed == LONG_MAX ||
            parsed < 0 ||
            (end && *end != '\0')) {
            ELOG("Could not parse con id \"%s\"\n", cvalue);
        } else {
            current_match->con_id = (Con*)parsed;
            printf("id as int = %p\n", current_match->con_id);
        }
        return NULL;
    }

    if (strcmp(ctype, "id") == 0) {
        char *end;
        long parsed = strtol(cvalue, &end, 10);
        if (parsed == LONG_MIN ||
            parsed == LONG_MAX ||
            parsed < 0 ||
            (end && *end != '\0')) {
            ELOG("Could not parse window id \"%s\"\n", cvalue);
        } else {
            current_match->id = parsed;
            printf("window id as int = %d\n", current_match->id);
        }
        return NULL;
    }

    if (strcmp(ctype, "con_mark") == 0) {
        current_match->mark = regex_new(cvalue);
        return NULL;
    }

    if (strcmp(ctype, "title") == 0) {
        current_match->title = regex_new(cvalue);
        return NULL;
    }

    ELOG("Unknown criterion: %s\n", ctype);

    /* This command is internal and does not generate a JSON reply. */
    return NULL;
}

char *cmd_move_con_to_workspace(Match *current_match, char *which) {
    owindow *current;

    DLOG("which=%s\n", which);

    HANDLE_EMPTY_MATCH;

    /* get the workspace */
    Con *ws;
    if (strcmp(which, "next") == 0)
        ws = workspace_next();
    else if (strcmp(which, "prev") == 0)
        ws = workspace_prev();
    else if (strcmp(which, "next_on_output") == 0)
        ws = workspace_next_on_output();
    else if (strcmp(which, "prev_on_output") == 0)
        ws = workspace_prev_on_output();
    else {
        ELOG("BUG: called with which=%s\n", which);
        return sstrdup("{\"sucess\": false}");
    }

    TAILQ_FOREACH(current, &owindows, owindows) {
        DLOG("matching: %p / %s\n", current->con, current->con->name);
        con_move_to_workspace(current->con, ws, true, false);
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_move_con_to_workspace_name(Match *current_match, char *name) {
    if (strncasecmp(name, "__i3_", strlen("__i3_")) == 0) {
        LOG("You cannot switch to the i3 internal workspaces.\n");
        return sstrdup("{\"sucess\": false}");
    }

    owindow *current;

    /* Error out early to not create a non-existing workspace (in
     * workspace_get()) if we are not actually able to move anything. */
    if (match_is_empty(current_match) && focused->type == CT_WORKSPACE)
        return sstrdup("{\"sucess\": false}");

    LOG("should move window to workspace %s\n", name);
    /* get the workspace */
    Con *ws = workspace_get(name, NULL);

    HANDLE_EMPTY_MATCH;

    TAILQ_FOREACH(current, &owindows, owindows) {
        DLOG("matching: %p / %s\n", current->con, current->con->name);
        con_move_to_workspace(current->con, ws, true, false);
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_resize(Match *current_match, char *way, char *direction, char *resize_px, char *resize_ppt) {
    /* resize <grow|shrink> <direction> [<px> px] [or <ppt> ppt] */
    DLOG("resizing in way %s, direction %s, px %s or ppt %s\n", way, direction, resize_px, resize_ppt);
    // TODO: We could either handle this in the parser itself as a separate token (and make the stack typed) or we need a better way to convert a string to a number with error checking
    int px = atoi(resize_px);
    int ppt = atoi(resize_ppt);
    if (strcmp(way, "shrink") == 0) {
        px *= -1;
        ppt *= -1;
    }

    Con *floating_con;
    if ((floating_con = con_inside_floating(focused))) {
        printf("floating resize\n");
        if (strcmp(direction, "up") == 0) {
            floating_con->rect.y -= px;
            floating_con->rect.height += px;
        } else if (strcmp(direction, "down") == 0) {
            floating_con->rect.height += px;
        } else if (strcmp(direction, "left") == 0) {
            floating_con->rect.x -= px;
            floating_con->rect.width += px;
        } else {
            floating_con->rect.width += px;
        }
    } else {
        LOG("tiling resize\n");
        /* get the appropriate current container (skip stacked/tabbed cons) */
        Con *current = focused;
        while (current->parent->layout == L_STACKED ||
               current->parent->layout == L_TABBED)
            current = current->parent;

        /* Then further go up until we find one with the matching orientation. */
        orientation_t search_orientation =
            (strcmp(direction, "left") == 0 || strcmp(direction, "right") == 0 ? HORIZ : VERT);

        while (current->type != CT_WORKSPACE &&
               current->type != CT_FLOATING_CON &&
               current->parent->orientation != search_orientation)
            current = current->parent;

        /* get the default percentage */
        int children = con_num_children(current->parent);
        Con *other;
        LOG("ins. %d children\n", children);
        double percentage = 1.0 / children;
        LOG("default percentage = %f\n", percentage);

        orientation_t orientation = current->parent->orientation;

        if ((orientation == HORIZ &&
             (strcmp(direction, "up") == 0 || strcmp(direction, "down") == 0)) ||
            (orientation == VERT &&
             (strcmp(direction, "left") == 0 || strcmp(direction, "right") == 0))) {
            LOG("You cannot resize in that direction. Your focus is in a %s split container currently.\n",
                (orientation == HORIZ ? "horizontal" : "vertical"));
            return sstrdup("{\"sucess\": false}");
        }

        if (strcmp(direction, "up") == 0 || strcmp(direction, "left") == 0) {
            other = TAILQ_PREV(current, nodes_head, nodes);
        } else {
            other = TAILQ_NEXT(current, nodes);
        }
        if (other == TAILQ_END(workspaces)) {
            LOG("No other container in this direction found, cannot resize.\n");
            return sstrdup("{\"sucess\": false}");
        }
        LOG("other->percent = %f\n", other->percent);
        LOG("current->percent before = %f\n", current->percent);
        if (current->percent == 0.0)
            current->percent = percentage;
        if (other->percent == 0.0)
            other->percent = percentage;
        double new_current_percent = current->percent + ((double)ppt / 100.0);
        double new_other_percent = other->percent - ((double)ppt / 100.0);
        LOG("new_current_percent = %f\n", new_current_percent);
        LOG("new_other_percent = %f\n", new_other_percent);
        /* Ensure that the new percentages are positive and greater than
         * 0.05 to have a reasonable minimum size. */
        if (definitelyGreaterThan(new_current_percent, 0.05, DBL_EPSILON) &&
            definitelyGreaterThan(new_other_percent, 0.05, DBL_EPSILON)) {
            current->percent += ((double)ppt / 100.0);
            other->percent -= ((double)ppt / 100.0);
            LOG("current->percent after = %f\n", current->percent);
            LOG("other->percent after = %f\n", other->percent);
        } else {
            LOG("Not resizing, already at minimum size\n");
        }
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_border(Match *current_match, char *border_style_str) {
    DLOG("border style should be changed to %s\n", border_style_str);
    owindow *current;

    HANDLE_EMPTY_MATCH;

    TAILQ_FOREACH(current, &owindows, owindows) {
        DLOG("matching: %p / %s\n", current->con, current->con->name);
        int border_style = current->con->border_style;
        if (strcmp(border_style_str, "toggle") == 0) {
            border_style++;
            border_style %= 3;
        } else {
            if (strcmp(border_style_str, "normal") == 0)
                border_style = BS_NORMAL;
            else if (strcmp(border_style_str, "none") == 0)
                border_style = BS_NONE;
            else if (strcmp(border_style_str, "1pixel") == 0)
                border_style = BS_1PIXEL;
            else {
                ELOG("BUG: called with border_style=%s\n", border_style_str);
                return sstrdup("{\"sucess\": false}");
            }
        }
        con_set_border_style(current->con, border_style);
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_nop(Match *current_match, char *comment) {
    LOG("-------------------------------------------------\n");
    LOG("  NOP: %s\n", comment);
    LOG("-------------------------------------------------\n");

    return NULL;
}

char *cmd_append_layout(Match *current_match, char *path) {
    LOG("Appending layout \"%s\"\n", path);
    tree_append_json(path);
    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_workspace(Match *current_match, char *which) {
    Con *ws;

    DLOG("which=%s\n", which);

    if (strcmp(which, "next") == 0)
        ws = workspace_next();
    else if (strcmp(which, "prev") == 0)
        ws = workspace_prev();
    else if (strcmp(which, "next_on_output") == 0)
        ws = workspace_next_on_output();
    else if (strcmp(which, "prev_on_output") == 0)
        ws = workspace_prev_on_output();
    else {
        ELOG("BUG: called with which=%s\n", which);
        return sstrdup("{\"sucess\": false}");
    }

    workspace_show(ws);
    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_workspace_back_and_forth(Match *current_match) {
    workspace_back_and_forth();
    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_workspace_name(Match *current_match, char *name) {
    if (strncasecmp(name, "__i3_", strlen("__i3_")) == 0) {
        LOG("You cannot switch to the i3 internal workspaces.\n");
        return sstrdup("{\"sucess\": false}");
    }

    DLOG("should switch to workspace %s\n", name);

    Con *ws = con_get_workspace(focused);

    /* Check if the command wants to switch to the current workspace */
    if (strcmp(ws->name, name) == 0) {
        DLOG("This workspace is already focused.\n");
        if (config.workspace_auto_back_and_forth) {
            workspace_back_and_forth();
            tree_render();
        }
        return sstrdup("{\"sucess\": false}");
    }

    workspace_show_by_name(name);

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_mark(Match *current_match, char *mark) {
    DLOG("Clearing all windows which have that mark first\n");

    Con *con;
    TAILQ_FOREACH(con, &all_cons, all_cons) {
        if (con->mark && strcmp(con->mark, mark) == 0)
            FREE(con->mark);
    }

    DLOG("marking window with str %s\n", mark);
    owindow *current;

    HANDLE_EMPTY_MATCH;

    TAILQ_FOREACH(current, &owindows, owindows) {
        DLOG("matching: %p / %s\n", current->con, current->con->name);
        current->con->mark = sstrdup(mark);
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_mode(Match *current_match, char *mode) {
    DLOG("mode=%s\n", mode);
    switch_mode(mode);

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_move_con_to_output(Match *current_match, char *name) {
    owindow *current;

    DLOG("should move window to output %s\n", name);

    HANDLE_EMPTY_MATCH;

    /* get the output */
    Output *current_output = NULL;
    Output *output;

    // TODO: fix the handling of criteria
    TAILQ_FOREACH(current, &owindows, owindows)
        current_output = get_output_containing(current->con->rect.x, current->con->rect.y);

    assert(current_output != NULL);

    // TODO: clean this up with commands.spec as soon as we switched away from the lex/yacc command parser
    if (strcasecmp(name, "up") == 0)
        output = get_output_next(D_UP, current_output);
    else if (strcasecmp(name, "down") == 0)
        output = get_output_next(D_DOWN, current_output);
    else if (strcasecmp(name, "left") == 0)
        output = get_output_next(D_LEFT, current_output);
    else if (strcasecmp(name, "right") == 0)
        output = get_output_next(D_RIGHT, current_output);
    else
        output = get_output_by_name(name);

    if (!output) {
        LOG("No such output found.\n");
        return sstrdup("{\"sucess\": false}");
    }

    /* get visible workspace on output */
    Con *ws = NULL;
    GREP_FIRST(ws, output_get_content(output->con), workspace_is_visible(child));
    if (!ws)
        return sstrdup("{\"sucess\": false}");

    TAILQ_FOREACH(current, &owindows, owindows) {
        DLOG("matching: %p / %s\n", current->con, current->con->name);
        con_move_to_workspace(current->con, ws, true, false);
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_floating(Match *current_match, char *floating_mode) {
    owindow *current;

    DLOG("floating_mode=%s\n", floating_mode);

    HANDLE_EMPTY_MATCH;

    TAILQ_FOREACH(current, &owindows, owindows) {
        DLOG("matching: %p / %s\n", current->con, current->con->name);
        if (strcmp(floating_mode, "toggle") == 0) {
            DLOG("should toggle mode\n");
            toggle_floating_mode(current->con, false);
        } else {
            DLOG("should switch mode to %s\n", floating_mode);
            if (strcmp(floating_mode, "enable") == 0) {
                floating_enable(current->con, false);
            } else {
                floating_disable(current->con, false);
            }
        }
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_move_workspace_to_output(Match *current_match, char *name) {
    DLOG("should move workspace to output %s\n", name);

    HANDLE_EMPTY_MATCH;

    owindow *current;
    TAILQ_FOREACH(current, &owindows, owindows) {
        Output *current_output = get_output_containing(current->con->rect.x,
                                                       current->con->rect.y);
        Output *output = get_output_from_string(current_output, name);
        if (!output) {
            LOG("No such output\n");
            return sstrdup("{\"sucess\": false}");
        }

        Con *content = output_get_content(output->con);
        LOG("got output %p with content %p\n", output, content);

        Con *ws = con_get_workspace(current->con);
        LOG("should move workspace %p / %s\n", ws, ws->name);
        if (con_num_children(ws->parent) == 1) {
            LOG("Not moving workspace \"%s\", it is the only workspace on its output.\n", ws->name);
            continue;
        }
        bool workspace_was_visible = workspace_is_visible(ws);
        Con *old_content = ws->parent;
        con_detach(ws);
        if (workspace_was_visible) {
            /* The workspace which we just detached was visible, so focus
             * the next one in the focus-stack. */
            Con *focus_ws = TAILQ_FIRST(&(old_content->focus_head));
            LOG("workspace was visible, focusing %p / %s now\n", focus_ws, focus_ws->name);
            workspace_show(focus_ws);
        }
        con_attach(ws, content, false);
        ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"move\"}");
        if (workspace_was_visible) {
            /* Focus the moved workspace on the destination output. */
            workspace_show(ws);
        }
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_split(Match *current_match, char *direction) {
    /* TODO: use matches */
    LOG("splitting in direction %c\n", direction[0]);
    tree_split(focused, (direction[0] == 'v' ? VERT : HORIZ));

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_kill(Match *current_match, char *kill_mode_str) {
    owindow *current;

    DLOG("kill_mode=%s\n", kill_mode_str);

    int kill_mode;
    if (kill_mode_str == NULL || strcmp(kill_mode_str, "window") == 0)
        kill_mode = KILL_WINDOW;
    else if (strcmp(kill_mode_str, "client") == 0)
        kill_mode = KILL_CLIENT;
    else {
        ELOG("BUG: called with kill_mode=%s\n", kill_mode_str);
        return sstrdup("{\"sucess\": false}");
    }

    /* check if the match is empty, not if the result is empty */
    if (match_is_empty(current_match))
        tree_close_con(kill_mode);
    else {
        TAILQ_FOREACH(current, &owindows, owindows) {
            DLOG("matching: %p / %s\n", current->con, current->con->name);
            tree_close(current->con, kill_mode, false, false);
        }
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_exec(Match *current_match, char *nosn, char *command) {
    bool no_startup_id = (nosn != NULL);

    DLOG("should execute %s, no_startup_id = %d\n", command, no_startup_id);
    start_application(command, no_startup_id);

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_focus_direction(Match *current_match, char *direction) {
    if (focused &&
        focused->type != CT_WORKSPACE &&
        focused->fullscreen_mode != CF_NONE) {
        LOG("Cannot change focus while in fullscreen mode.\n");
        return sstrdup("{\"sucess\": false}");
    }

    DLOG("direction = *%s*\n", direction);

    if (strcmp(direction, "left") == 0)
        tree_next('p', HORIZ);
    else if (strcmp(direction, "right") == 0)
        tree_next('n', HORIZ);
    else if (strcmp(direction, "up") == 0)
        tree_next('p', VERT);
    else if (strcmp(direction, "down") == 0)
        tree_next('n', VERT);
    else {
        ELOG("Invalid focus direction (%s)\n", direction);
        return sstrdup("{\"sucess\": false}");
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_focus_window_mode(Match *current_match, char *window_mode) {
    if (focused &&
        focused->type != CT_WORKSPACE &&
        focused->fullscreen_mode != CF_NONE) {
        LOG("Cannot change focus while in fullscreen mode.\n");
        return sstrdup("{\"sucess\": false}");
    }

    DLOG("window_mode = %s\n", window_mode);

    Con *ws = con_get_workspace(focused);
    Con *current;
    if (ws != NULL) {
        if (strcmp(window_mode, "mode_toggle") == 0) {
            current = TAILQ_FIRST(&(ws->focus_head));
            if (current != NULL && current->type == CT_FLOATING_CON)
                window_mode = "tiling";
            else window_mode = "floating";
        }
        TAILQ_FOREACH(current, &(ws->focus_head), focused) {
            if ((strcmp(window_mode, "floating") == 0 && current->type != CT_FLOATING_CON) ||
                (strcmp(window_mode, "tiling") == 0 && current->type == CT_FLOATING_CON))
                continue;

            con_focus(con_descend_focused(current));
            break;
        }
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_focus_level(Match *current_match, char *level) {
    if (focused &&
        focused->type != CT_WORKSPACE &&
        focused->fullscreen_mode != CF_NONE) {
        LOG("Cannot change focus while in fullscreen mode.\n");
        return sstrdup("{\"sucess\": false}");
    }

    DLOG("level = %s\n", level);

    if (strcmp(level, "parent") == 0)
        level_up();
    else level_down();

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_focus(Match *current_match) {
    DLOG("current_match = %p\n", current_match);
    if (focused &&
        focused->type != CT_WORKSPACE &&
        focused->fullscreen_mode != CF_NONE) {
        LOG("Cannot change focus while in fullscreen mode.\n");
        return sstrdup("{\"sucess\": false}");
    }

    owindow *current;

    if (match_is_empty(current_match)) {
        ELOG("You have to specify which window/container should be focused.\n");
        ELOG("Example: [class=\"urxvt\" title=\"irssi\"] focus\n");

        // TODO: json output
        char *json_output;
        sasprintf(&json_output, "{\"success\":false, \"error\":\"You have to "
                  "specify which window/container should be focused\"}");
        return json_output;
    }

    LOG("here");
    int count = 0;
    TAILQ_FOREACH(current, &owindows, owindows) {
        Con *ws = con_get_workspace(current->con);
        /* If no workspace could be found, this was a dock window.
         * Just skip it, you cannot focus dock windows. */
        if (!ws)
            continue;
    LOG("there");

        /* If the container is not on the current workspace,
         * workspace_show() will switch to a different workspace and (if
         * enabled) trigger a mouse pointer warp to the currently focused
         * container (!) on the target workspace.
         *
         * Therefore, before calling workspace_show(), we make sure that
         * 'current' will be focused on the workspace. However, we cannot
         * just con_focus(current) because then the pointer will not be
         * warped at all (the code thinks we are already there).
         *
         * So we focus 'current' to make it the currently focused window of
         * the target workspace, then revert focus. */
        Con *currently_focused = focused;
        con_focus(current->con);
        con_focus(currently_focused);

        /* Now switch to the workspace, then focus */
        workspace_show(ws);
        LOG("focusing %p / %s\n", current->con, current->con->name);
        con_focus(current->con);
        count++;
    }

    if (count > 1)
        LOG("WARNING: Your criteria for the focus command matches %d containers, "
            "while only exactly one container can be focused at a time.\n", count);

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_fullscreen(Match *current_match, char *fullscreen_mode) {
    DLOG("toggling fullscreen, mode = %s\n", fullscreen_mode);
    owindow *current;

    HANDLE_EMPTY_MATCH;

    TAILQ_FOREACH(current, &owindows, owindows) {
        printf("matching: %p / %s\n", current->con, current->con->name);
        con_toggle_fullscreen(current->con, (fullscreen_mode && strcmp(fullscreen_mode, "global") == 0 ? CF_GLOBAL : CF_OUTPUT));
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_move_direction(Match *current_match, char *direction, char *move_px) {
    // TODO: We could either handle this in the parser itself as a separate token (and make the stack typed) or we need a better way to convert a string to a number with error checking
    int px = atoi(move_px);

    /* TODO: make 'move' work with criteria. */
    DLOG("moving in direction %s, px %s\n", direction, move_px);
    if (con_is_floating(focused)) {
        DLOG("floating move with %d pixels\n", px);
        Rect newrect = focused->parent->rect;
        if (strcmp(direction, "left") == 0) {
            newrect.x -= px;
        } else if (strcmp(direction, "right") == 0) {
            newrect.x += px;
        } else if (strcmp(direction, "up") == 0) {
            newrect.y -= px;
        } else if (strcmp(direction, "down") == 0) {
            newrect.y += px;
        }
        floating_reposition(focused->parent, newrect);
    } else {
        tree_move((strcmp(direction, "right") == 0 ? TOK_RIGHT :
                   (strcmp(direction, "left") == 0 ? TOK_LEFT :
                    (strcmp(direction, "up") == 0 ? TOK_UP :
                     TOK_DOWN))));
        tree_render();
    }


    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_layout(Match *current_match, char *layout_str) {
    DLOG("changing layout to %s\n", layout_str);
    owindow *current;
    int layout = (strcmp(layout_str, "default") == 0 ? L_DEFAULT :
                  (strcmp(layout_str, "stacked") == 0 || strcmp(layout_str, "stacking") == 0 ? L_STACKED :
                   L_TABBED));

    /* check if the match is empty, not if the result is empty */
    if (match_is_empty(current_match))
        con_set_layout(focused->parent, layout);
    else {
        TAILQ_FOREACH(current, &owindows, owindows) {
            DLOG("matching: %p / %s\n", current->con, current->con->name);
            con_set_layout(current->con, layout);
        }
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_exit(Match *current_match) {
    LOG("Exiting due to user command.\n");
    exit(0);

    /* unreached */
}

char *cmd_reload(Match *current_match) {
    LOG("reloading\n");
    kill_configerror_nagbar(false);
    load_configuration(conn, NULL, true);
    x_set_i3_atoms();
    /* Send an IPC event just in case the ws names have changed */
    ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"reload\"}");

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_restart(Match *current_match) {
    LOG("restarting i3\n");
    i3_restart(false);

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_open(Match *current_match) {
    LOG("opening new container\n");
    Con *con = tree_open_con(NULL, NULL);
    con_focus(con);
    char *json_output;
    sasprintf(&json_output, "{\"success\":true, \"id\":%ld}", (long int)con);

    tree_render();

    return json_output;
}

char *cmd_focus_output(Match *current_match, char *name) {
    owindow *current;

    DLOG("name = %s\n", name);

    HANDLE_EMPTY_MATCH;

    /* get the output */
    Output *current_output = NULL;
    Output *output;

    TAILQ_FOREACH(current, &owindows, owindows)
        current_output = get_output_containing(current->con->rect.x, current->con->rect.y);
    assert(current_output != NULL);

    output = get_output_from_string(current_output, name);

    if (!output) {
        LOG("No such output found.\n");
        return sstrdup("{\"sucess\": false}");
    }

    /* get visible workspace on output */
    Con *ws = NULL;
    GREP_FIRST(ws, output_get_content(output->con), workspace_is_visible(child));
    if (!ws)
        return sstrdup("{\"sucess\": false}");

    workspace_show(ws);
    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_move_scratchpad(Match *current_match) {
    DLOG("should move window to scratchpad\n");
    owindow *current;

    HANDLE_EMPTY_MATCH;

    TAILQ_FOREACH(current, &owindows, owindows) {
        DLOG("matching: %p / %s\n", current->con, current->con->name);
        scratchpad_move(current->con);
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}

char *cmd_scratchpad_show(Match *current_match) {
    DLOG("should show scratchpad window\n");
    owindow *current;

    if (match_is_empty(current_match)) {
        scratchpad_show(NULL);
    } else {
        TAILQ_FOREACH(current, &owindows, owindows) {
            DLOG("matching: %p / %s\n", current->con, current->con->name);
            scratchpad_show(current->con);
        }
    }

    tree_render();

    // XXX: default reply for now, make this a better reply
    return sstrdup("{\"success\": true}");
}
