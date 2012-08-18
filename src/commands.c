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
#include <stdarg.h>

#include "all.h"

// Macros to make the YAJL API a bit easier to use.
#define y(x, ...) yajl_gen_ ## x (cmd_output->json_gen, ##__VA_ARGS__)
#define ystr(str) yajl_gen_string(cmd_output->json_gen, (unsigned char*)str, strlen(str))
#define ysuccess(success) do { \
    y(map_open); \
    ystr("success"); \
    y(bool, success); \
    y(map_close); \
} while (0)

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

/*
 * Returns an 'output' corresponding to one of left/right/down/up or a specific
 * output name.
 *
 */
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

/*
 * Checks whether we switched to a new workspace and returns false in that case,
 * signaling that further workspace switching should be done by the calling function
 * If not, calls workspace_back_and_forth() if workspace_auto_back_and_forth is set
 * and return true, signaling that no further workspace switching should occur in the calling function.
 *
 */
static bool maybe_back_and_forth(struct CommandResult *cmd_output, char *name) {
    Con *ws = con_get_workspace(focused);

    /* If we switched to a different workspace, do nothing */
    if (strcmp(ws->name, name) != 0)
        return false;

    DLOG("This workspace is already focused.\n");
    if (config.workspace_auto_back_and_forth) {
        workspace_back_and_forth();
        cmd_output->needs_tree_render = true;
    }
    return true;
}

// This code is commented out because we might recycle it for popping up error
// messages on parser errors.
#if 0
static pid_t migration_pid = -1;

/*
 * Handler which will be called when we get a SIGCHLD for the nagbar, meaning
 * it exited (or could not be started, depending on the exit code).
 *
 */
static void nagbar_exited(EV_P_ ev_child *watcher, int revents) {
    ev_child_stop(EV_A_ watcher);
    if (!WIFEXITED(watcher->rstatus)) {
        fprintf(stderr, "ERROR: i3-nagbar did not exit normally.\n");
        return;
    }

    int exitcode = WEXITSTATUS(watcher->rstatus);
    printf("i3-nagbar process exited with status %d\n", exitcode);
    if (exitcode == 2) {
        fprintf(stderr, "ERROR: i3-nagbar could not be found. Is it correctly installed on your system?\n");
    }

    migration_pid = -1;
}

/* We need ev >= 4 for the following code. Since it is not *that* important (it
 * only makes sure that there are no i3-nagbar instances left behind) we still
 * support old systems with libev 3. */
#if EV_VERSION_MAJOR >= 4
/*
 * Cleanup handler. Will be called when i3 exits. Kills i3-nagbar with signal
 * SIGKILL (9) to make sure there are no left-over i3-nagbar processes.
 *
 */
static void nagbar_cleanup(EV_P_ ev_cleanup *watcher, int revent) {
    if (migration_pid != -1) {
        LOG("Sending SIGKILL (9) to i3-nagbar with PID %d\n", migration_pid);
        kill(migration_pid, SIGKILL);
    }
}
#endif

void cmd_MIGRATION_start_nagbar(void) {
    if (migration_pid != -1) {
        fprintf(stderr, "i3-nagbar already running.\n");
        return;
    }
    fprintf(stderr, "Starting i3-nagbar, command parsing differs from expected output.\n");
    ELOG("Please report this on IRC or in the bugtracker. Make sure to include the full debug level logfile:\n");
    ELOG("i3-dump-log | gzip -9c > /tmp/i3.log.gz\n");
    ELOG("FYI: Your i3 version is " I3_VERSION "\n");
    migration_pid = fork();
    if (migration_pid == -1) {
        warn("Could not fork()");
        return;
    }

    /* child */
    if (migration_pid == 0) {
        char *pageraction;
        sasprintf(&pageraction, "i3-sensible-terminal -e i3-sensible-pager \"%s\"", errorfilename);
        char *argv[] = {
            NULL, /* will be replaced by the executable path */
            "-t",
            "error",
            "-m",
            "You found a parsing error. Please, please, please, report it!",
            "-b",
            "show errors",
            pageraction,
            NULL
        };
        exec_i3_utility("i3-nagbar", argv);
    }

    /* parent */
    /* install a child watcher */
    ev_child *child = smalloc(sizeof(ev_child));
    ev_child_init(child, &nagbar_exited, migration_pid, 0);
    ev_child_start(main_loop, child);

/* We need ev >= 4 for the following code. Since it is not *that* important (it
 * only makes sure that there are no i3-nagbar instances left behind) we still
 * support old systems with libev 3. */
#if EV_VERSION_MAJOR >= 4
    /* install a cleanup watcher (will be called when i3 exits and i3-nagbar is
     * still running) */
    ev_cleanup *cleanup = smalloc(sizeof(ev_cleanup));
    ev_cleanup_init(cleanup, nagbar_cleanup);
    ev_cleanup_start(main_loop, cleanup);
#endif
}

#endif

/*******************************************************************************
 * Criteria functions.
 ******************************************************************************/

/*
 * Initializes the specified 'Match' data structure and the initial state of
 * commands.c for matching target windows of a command.
 *
 */
void cmd_criteria_init(I3_CMD) {
    Con *con;
    owindow *ow;

    DLOG("Initializing criteria, current_match = %p\n", current_match);
    match_init(current_match);
    while (!TAILQ_EMPTY(&owindows)) {
        ow = TAILQ_FIRST(&owindows);
        TAILQ_REMOVE(&owindows, ow, owindows);
        free(ow);
    }
    TAILQ_INIT(&owindows);
    /* copy all_cons */
    TAILQ_FOREACH(con, &all_cons, all_cons) {
        ow = smalloc(sizeof(owindow));
        ow->con = con;
        TAILQ_INSERT_TAIL(&owindows, ow, owindows);
    }
}

/*
 * A match specification just finished (the closing square bracket was found),
 * so we filter the list of owindows.
 *
 */
void cmd_criteria_match_windows(I3_CMD) {
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
}

/*
 * Interprets a ctype=cvalue pair and adds it to the current match
 * specification.
 *
 */
void cmd_criteria_add(I3_CMD, char *ctype, char *cvalue) {
    DLOG("ctype=*%s*, cvalue=*%s*\n", ctype, cvalue);

    if (strcmp(ctype, "class") == 0) {
        current_match->class = regex_new(cvalue);
        return;
    }

    if (strcmp(ctype, "instance") == 0) {
        current_match->instance = regex_new(cvalue);
        return;
    }

    if (strcmp(ctype, "window_role") == 0) {
        current_match->role = regex_new(cvalue);
        return;
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
        return;
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
        return;
    }

    if (strcmp(ctype, "con_mark") == 0) {
        current_match->mark = regex_new(cvalue);
        return;
    }

    if (strcmp(ctype, "title") == 0) {
        current_match->title = regex_new(cvalue);
        return;
    }

    if (strcmp(ctype, "urgent") == 0) {
        if (strcasecmp(cvalue, "latest") == 0 ||
            strcasecmp(cvalue, "newest") == 0 ||
            strcasecmp(cvalue, "recent") == 0 ||
            strcasecmp(cvalue, "last") == 0) {
            current_match->urgent = U_LATEST;
        } else if (strcasecmp(cvalue, "oldest") == 0 ||
                   strcasecmp(cvalue, "first") == 0) {
            current_match->urgent = U_OLDEST;
        }
        return;
    }

    ELOG("Unknown criterion: %s\n", ctype);
}

/*
 * Implementation of 'move [window|container] [to] workspace
 * next|prev|next_on_output|prev_on_output'.
 *
 */
void cmd_move_con_to_workspace(I3_CMD, char *which) {
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
        ysuccess(false);
        return;
    }

    TAILQ_FOREACH(current, &owindows, owindows) {
        DLOG("matching: %p / %s\n", current->con, current->con->name);
        con_move_to_workspace(current->con, ws, true, false);
    }

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'move [window|container] [to] workspace <name>'.
 *
 */
void cmd_move_con_to_workspace_name(I3_CMD, char *name) {
    if (strncasecmp(name, "__i3_", strlen("__i3_")) == 0) {
        LOG("You cannot switch to the i3 internal workspaces.\n");
        ysuccess(false);
        return;
    }

    owindow *current;

    /* Error out early to not create a non-existing workspace (in
     * workspace_get()) if we are not actually able to move anything. */
    if (match_is_empty(current_match) && focused->type == CT_WORKSPACE) {
        ysuccess(false);
        return;
    }

    LOG("should move window to workspace %s\n", name);
    /* get the workspace */
    Con *ws = workspace_get(name, NULL);

    HANDLE_EMPTY_MATCH;

    TAILQ_FOREACH(current, &owindows, owindows) {
        DLOG("matching: %p / %s\n", current->con, current->con->name);
        con_move_to_workspace(current->con, ws, true, false);
    }

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'move [window|container] [to] workspace number <number>'.
 *
 */
void cmd_move_con_to_workspace_number(I3_CMD, char *which) {
    owindow *current;

    /* Error out early to not create a non-existing workspace (in
     * workspace_get()) if we are not actually able to move anything. */
    if (match_is_empty(current_match) && focused->type == CT_WORKSPACE) {
        ysuccess(false);
        return;
    }

    LOG("should move window to workspace with number %d\n", which);
    /* get the workspace */
    Con *output, *workspace = NULL;

    char *endptr = NULL;
    long parsed_num = strtol(which, &endptr, 10);
    if (parsed_num == LONG_MIN ||
        parsed_num == LONG_MAX ||
        parsed_num < 0 ||
        *endptr != '\0') {
        LOG("Could not parse \"%s\" as a number.\n", which);
        y(map_open);
        ystr("success");
        y(bool, false);
        ystr("error");
        // TODO: better error message
        ystr("Could not parse number");
        y(map_close);
        return;
    }

    TAILQ_FOREACH(output, &(croot->nodes_head), nodes)
        GREP_FIRST(workspace, output_get_content(output),
            child->num == parsed_num);

    if (!workspace) {
        y(map_open);
        ystr("success");
        y(bool, false);
        ystr("error");
        // TODO: better error message
        ystr("No such workspace");
        y(map_close);
        return;
    }

    HANDLE_EMPTY_MATCH;

    TAILQ_FOREACH(current, &owindows, owindows) {
        DLOG("matching: %p / %s\n", current->con, current->con->name);
        con_move_to_workspace(current->con, workspace, true, false);
    }

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

static void cmd_resize_floating(I3_CMD, char *way, char *direction, Con *floating_con, int px) {
    LOG("floating resize\n");
    if (strcmp(direction, "up") == 0) {
        floating_con->rect.y -= px;
        floating_con->rect.height += px;
    } else if (strcmp(direction, "down") == 0 || strcmp(direction, "height") == 0) {
        floating_con->rect.height += px;
    } else if (strcmp(direction, "left") == 0) {
        floating_con->rect.x -= px;
        floating_con->rect.width += px;
    } else {
        floating_con->rect.width += px;
    }
}

static bool cmd_resize_tiling_direction(I3_CMD, char *way, char *direction, int ppt) {
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
        ysuccess(false);
        return false;
    }

    if (strcmp(direction, "up") == 0 || strcmp(direction, "left") == 0) {
        other = TAILQ_PREV(current, nodes_head, nodes);
    } else {
        other = TAILQ_NEXT(current, nodes);
    }
    if (other == TAILQ_END(workspaces)) {
        LOG("No other container in this direction found, cannot resize.\n");
        ysuccess(false);
        return false;
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

    return true;
}

static bool cmd_resize_tiling_width_height(I3_CMD, char *way, char *direction, int ppt) {
    LOG("width/height resize\n");
    /* get the appropriate current container (skip stacked/tabbed cons) */
    Con *current = focused;
    while (current->parent->layout == L_STACKED ||
           current->parent->layout == L_TABBED)
        current = current->parent;

    /* Then further go up until we find one with the matching orientation. */
    orientation_t search_orientation =
        (strcmp(direction, "width") == 0 ? HORIZ : VERT);

    while (current->type != CT_WORKSPACE &&
           current->type != CT_FLOATING_CON &&
           current->parent->orientation != search_orientation)
        current = current->parent;

    /* get the default percentage */
    int children = con_num_children(current->parent);
    LOG("ins. %d children\n", children);
    double percentage = 1.0 / children;
    LOG("default percentage = %f\n", percentage);

    orientation_t orientation = current->parent->orientation;

    if ((orientation == HORIZ &&
         strcmp(direction, "height") == 0) ||
        (orientation == VERT &&
         strcmp(direction, "width") == 0)) {
        LOG("You cannot resize in that direction. Your focus is in a %s split container currently.\n",
            (orientation == HORIZ ? "horizontal" : "vertical"));
        ysuccess(false);
        return false;
    }

    if (children == 1) {
        LOG("This is the only container, cannot resize.\n");
        ysuccess(false);
        return false;
    }

    /* Ensure all the other children have a percentage set. */
    Con *child;
    TAILQ_FOREACH(child, &(current->parent->nodes_head), nodes) {
        LOG("child->percent = %f (child %p)\n", child->percent, child);
        if (child->percent == 0.0)
            child->percent = percentage;
    }

    double new_current_percent = current->percent + ((double)ppt / 100.0);
    double subtract_percent = ((double)ppt / 100.0) / (children - 1);
    LOG("new_current_percent = %f\n", new_current_percent);
    LOG("subtract_percent = %f\n", subtract_percent);
    /* Ensure that the new percentages are positive and greater than
     * 0.05 to have a reasonable minimum size. */
    TAILQ_FOREACH(child, &(current->parent->nodes_head), nodes) {
        if (child == current)
            continue;
        if (!definitelyGreaterThan(child->percent - subtract_percent, 0.05, DBL_EPSILON)) {
            LOG("Not resizing, already at minimum size (child %p would end up with a size of %.f\n", child, child->percent - subtract_percent);
            ysuccess(false);
            return false;
        }
    }
    if (!definitelyGreaterThan(new_current_percent, 0.05, DBL_EPSILON)) {
        LOG("Not resizing, already at minimum size\n");
        ysuccess(false);
        return false;
    }

    current->percent += ((double)ppt / 100.0);
    LOG("current->percent after = %f\n", current->percent);

    TAILQ_FOREACH(child, &(current->parent->nodes_head), nodes) {
        if (child == current)
            continue;
        child->percent -= subtract_percent;
        LOG("child->percent after (%p) = %f\n", child, child->percent);
    }

    return true;
}

/*
 * Implementation of 'resize grow|shrink <direction> [<px> px] [or <ppt> ppt]'.
 *
 */
void cmd_resize(I3_CMD, char *way, char *direction, char *resize_px, char *resize_ppt) {
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
        cmd_resize_floating(current_match, cmd_output, way, direction, floating_con, px);
    } else {
        if (strcmp(direction, "width") == 0 ||
            strcmp(direction, "height") == 0) {
            if (!cmd_resize_tiling_width_height(current_match, cmd_output, way, direction, ppt))
                return;
        } else {
            if (!cmd_resize_tiling_direction(current_match, cmd_output, way, direction, ppt))
                return;
        }
    }

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'border normal|none|1pixel|toggle'.
 *
 */
void cmd_border(I3_CMD, char *border_style_str) {
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
                ysuccess(false);
                return;
            }
        }
        con_set_border_style(current->con, border_style);
    }

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'nop <comment>'.
 *
 */
void cmd_nop(I3_CMD, char *comment) {
    LOG("-------------------------------------------------\n");
    LOG("  NOP: %s\n", comment);
    LOG("-------------------------------------------------\n");
}

/*
 * Implementation of 'append_layout <path>'.
 *
 */
void cmd_append_layout(I3_CMD, char *path) {
    LOG("Appending layout \"%s\"\n", path);
    tree_append_json(path);

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'workspace next|prev|next_on_output|prev_on_output'.
 *
 */
void cmd_workspace(I3_CMD, char *which) {
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
        ysuccess(false);
        return;
    }

    workspace_show(ws);

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'workspace number <number>'
 *
 */
void cmd_workspace_number(I3_CMD, char *which) {
    Con *output, *workspace = NULL;

    char *endptr = NULL;
    long parsed_num = strtol(which, &endptr, 10);
    if (parsed_num == LONG_MIN ||
        parsed_num == LONG_MAX ||
        parsed_num < 0 ||
        *endptr != '\0') {
        LOG("Could not parse \"%s\" as a number.\n", which);
        y(map_open);
        ystr("success");
        y(bool, false);
        ystr("error");
        // TODO: better error message
        ystr("Could not parse number");
        y(map_close);

        return;
    }

    TAILQ_FOREACH(output, &(croot->nodes_head), nodes)
        GREP_FIRST(workspace, output_get_content(output),
            child->num == parsed_num);

    if (!workspace) {
        LOG("There is no workspace with number %d, creating a new one.\n", parsed_num);
        ysuccess(true);
        /* terminate the which string after the endposition of the number */
        *endptr = '\0';
        if (maybe_back_and_forth(cmd_output, which))
            return;
        workspace_show_by_name(which);
        cmd_output->needs_tree_render = true;
        return;
    }
    if (maybe_back_and_forth(cmd_output, which))
        return;
    workspace_show(workspace);

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'workspace back_and_forth'.
 *
 */
void cmd_workspace_back_and_forth(I3_CMD) {
    workspace_back_and_forth();

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'workspace <name>'
 *
 */
void cmd_workspace_name(I3_CMD, char *name) {
    if (strncasecmp(name, "__i3_", strlen("__i3_")) == 0) {
        LOG("You cannot switch to the i3 internal workspaces.\n");
        ysuccess(false);
        return;
    }

    DLOG("should switch to workspace %s\n", name);
    if (maybe_back_and_forth(cmd_output, name))
       return;
    workspace_show_by_name(name);

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'mark <mark>'
 *
 */
void cmd_mark(I3_CMD, char *mark) {
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

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'mode <string>'.
 *
 */
void cmd_mode(I3_CMD, char *mode) {
    DLOG("mode=%s\n", mode);
    switch_mode(mode);

    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'move [window|container] [to] output <str>'.
 *
 */
void cmd_move_con_to_output(I3_CMD, char *name) {
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
        ysuccess(false);
        return;
    }

    /* get visible workspace on output */
    Con *ws = NULL;
    GREP_FIRST(ws, output_get_content(output->con), workspace_is_visible(child));
    if (!ws) {
        ysuccess(false);
        return;
    }

    TAILQ_FOREACH(current, &owindows, owindows) {
        DLOG("matching: %p / %s\n", current->con, current->con->name);
        con_move_to_workspace(current->con, ws, true, false);
    }

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'floating enable|disable|toggle'
 *
 */
void cmd_floating(I3_CMD, char *floating_mode) {
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

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'move workspace to [output] <str>'.
 *
 */
void cmd_move_workspace_to_output(I3_CMD, char *name) {
    DLOG("should move workspace to output %s\n", name);

    HANDLE_EMPTY_MATCH;

    owindow *current;
    TAILQ_FOREACH(current, &owindows, owindows) {
        Output *current_output = get_output_containing(current->con->rect.x,
                                                       current->con->rect.y);
        if (!current_output) {
            ELOG("Cannot get current output. This is a bug in i3.\n");
            ysuccess(false);
            return;
        }
        Output *output = get_output_from_string(current_output, name);
        if (!output) {
            ELOG("Could not get output from string \"%s\"\n", name);
            ysuccess(false);
            return;
        }

        Con *content = output_get_content(output->con);
        LOG("got output %p with content %p\n", output, content);

        Con *ws = con_get_workspace(current->con);
        LOG("should move workspace %p / %s\n", ws, ws->name);

        if (con_num_children(ws->parent) == 1) {
            LOG("Creating a new workspace to replace \"%s\" (last on its output).\n", ws->name);

            /* check if we can find a workspace assigned to this output */
            bool used_assignment = false;
            struct Workspace_Assignment *assignment;
            TAILQ_FOREACH(assignment, &ws_assignments, ws_assignments) {
                if (strcmp(assignment->output, current_output->name) != 0)
                    continue;

                /* check if this workspace is already attached to the tree */
                Con *workspace = NULL, *out;
                TAILQ_FOREACH(out, &(croot->nodes_head), nodes)
                    GREP_FIRST(workspace, output_get_content(out),
                               !strcasecmp(child->name, assignment->name));
                if (workspace != NULL)
                    continue;

                /* so create the workspace referenced to by this assignment */
                LOG("Creating workspace from assignment %s.\n", assignment->name);
                workspace_get(assignment->name, NULL);
                used_assignment = true;
                break;
            }

            /* if we couldn't create the workspace using an assignment, create
             * it on the output */
            if (!used_assignment)
                create_workspace_on_output(current_output, ws->parent);

            /* notify the IPC listeners */
            ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"init\"}");
        }

        /* detach from the old output and attach to the new output */
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

        /* fix the coordinates of the floating containers */
        Con *floating_con;
        TAILQ_FOREACH(floating_con, &(ws->floating_head), floating_windows)
            floating_fix_coordinates(floating_con, &(old_content->rect), &(content->rect));

        ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"move\"}");
        if (workspace_was_visible) {
            /* Focus the moved workspace on the destination output. */
            workspace_show(ws);
        }
    }

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'split v|h|vertical|horizontal'.
 *
 */
void cmd_split(I3_CMD, char *direction) {
    /* TODO: use matches */
    LOG("splitting in direction %c\n", direction[0]);
    tree_split(focused, (direction[0] == 'v' ? VERT : HORIZ));

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementaiton of 'kill [window|client]'.
 *
 */
void cmd_kill(I3_CMD, char *kill_mode_str) {
    if (kill_mode_str == NULL)
        kill_mode_str = "window";
    owindow *current;

    DLOG("kill_mode=%s\n", kill_mode_str);

    int kill_mode;
    if (strcmp(kill_mode_str, "window") == 0)
        kill_mode = KILL_WINDOW;
    else if (strcmp(kill_mode_str, "client") == 0)
        kill_mode = KILL_CLIENT;
    else {
        ELOG("BUG: called with kill_mode=%s\n", kill_mode_str);
        ysuccess(false);
        return;
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

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'exec [--no-startup-id] <command>'.
 *
 */
void cmd_exec(I3_CMD, char *nosn, char *command) {
    bool no_startup_id = (nosn != NULL);

    DLOG("should execute %s, no_startup_id = %d\n", command, no_startup_id);
    start_application(command, no_startup_id);

    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'focus left|right|up|down'.
 *
 */
void cmd_focus_direction(I3_CMD, char *direction) {
    if (focused &&
        focused->type != CT_WORKSPACE &&
        focused->fullscreen_mode != CF_NONE) {
        LOG("Cannot change focus while in fullscreen mode.\n");
        ysuccess(false);
        return;
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
        ysuccess(false);
        return;
    }

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'focus tiling|floating|mode_toggle'.
 *
 */
void cmd_focus_window_mode(I3_CMD, char *window_mode) {
    if (focused &&
        focused->type != CT_WORKSPACE &&
        focused->fullscreen_mode != CF_NONE) {
        LOG("Cannot change focus while in fullscreen mode.\n");
        ysuccess(false);
        return;
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

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'focus parent|child'.
 *
 */
void cmd_focus_level(I3_CMD, char *level) {
    if (focused &&
        focused->type != CT_WORKSPACE &&
        focused->fullscreen_mode != CF_NONE) {
        LOG("Cannot change focus while in fullscreen mode.\n");
        ysuccess(false);
        return;
    }

    DLOG("level = %s\n", level);

    if (strcmp(level, "parent") == 0)
        level_up();
    else level_down();

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'focus'.
 *
 */
void cmd_focus(I3_CMD) {
    DLOG("current_match = %p\n", current_match);

    if (match_is_empty(current_match)) {
        ELOG("You have to specify which window/container should be focused.\n");
        ELOG("Example: [class=\"urxvt\" title=\"irssi\"] focus\n");

        y(map_open);
        ystr("success");
        y(bool, false);
        ystr("error");
        ystr("You have to specify which window/container should be focused");
        y(map_close);

        return;
    }

    int count = 0;
    owindow *current;
    TAILQ_FOREACH(current, &owindows, owindows) {
        Con *ws = con_get_workspace(current->con);
        /* If no workspace could be found, this was a dock window.
         * Just skip it, you cannot focus dock windows. */
        if (!ws)
            continue;

        /* Don't allow the focus switch if the focused and current
         * containers are in the same workspace. */
        if (focused &&
            focused->type != CT_WORKSPACE &&
            focused->fullscreen_mode != CF_NONE &&
            con_get_workspace(focused) == ws) {
            LOG("Cannot change focus while in fullscreen mode (same workspace).\n");
            ysuccess(false);
            return;
        }

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

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'fullscreen [global]'.
 *
 */
void cmd_fullscreen(I3_CMD, char *fullscreen_mode) {
    if (fullscreen_mode == NULL)
        fullscreen_mode = "output";
    DLOG("toggling fullscreen, mode = %s\n", fullscreen_mode);
    owindow *current;

    HANDLE_EMPTY_MATCH;

    TAILQ_FOREACH(current, &owindows, owindows) {
        printf("matching: %p / %s\n", current->con, current->con->name);
        con_toggle_fullscreen(current->con, (strcmp(fullscreen_mode, "global") == 0 ? CF_GLOBAL : CF_OUTPUT));
    }

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'move <direction> [<pixels> [px]]'.
 *
 */
void cmd_move_direction(I3_CMD, char *direction, char *move_px) {
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
        tree_move((strcmp(direction, "right") == 0 ? D_RIGHT :
                   (strcmp(direction, "left") == 0 ? D_LEFT :
                    (strcmp(direction, "up") == 0 ? D_UP :
                     D_DOWN))));
        cmd_output->needs_tree_render = true;
    }

    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'layout default|stacked|stacking|tabbed'.
 *
 */
void cmd_layout(I3_CMD, char *layout_str) {
    if (strcmp(layout_str, "stacking") == 0)
        layout_str = "stacked";
    DLOG("changing layout to %s\n", layout_str);
    owindow *current;
    int layout = (strcmp(layout_str, "default") == 0 ? L_DEFAULT :
                  (strcmp(layout_str, "stacked") == 0 ? L_STACKED :
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

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementaiton of 'exit'.
 *
 */
void cmd_exit(I3_CMD) {
    LOG("Exiting due to user command.\n");
    exit(0);

    /* unreached */
}

/*
 * Implementaiton of 'reload'.
 *
 */
void cmd_reload(I3_CMD) {
    LOG("reloading\n");
    kill_configerror_nagbar(false);
    load_configuration(conn, NULL, true);
    x_set_i3_atoms();
    /* Send an IPC event just in case the ws names have changed */
    ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"reload\"}");

    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementaiton of 'restart'.
 *
 */
void cmd_restart(I3_CMD) {
    LOG("restarting i3\n");
    i3_restart(false);

    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementaiton of 'open'.
 *
 */
void cmd_open(I3_CMD) {
    LOG("opening new container\n");
    Con *con = tree_open_con(NULL, NULL);
    con_focus(con);

    y(map_open);
    ystr("success");
    y(bool, true);
    ystr("id");
    y(integer, (long int)con);
    y(map_close);

    cmd_output->needs_tree_render = true;
}

/*
 * Implementation of 'focus output <output>'.
 *
 */
void cmd_focus_output(I3_CMD, char *name) {
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
        ysuccess(false);
        return;
    }

    /* get visible workspace on output */
    Con *ws = NULL;
    GREP_FIRST(ws, output_get_content(output->con), workspace_is_visible(child));
    if (!ws) {
        ysuccess(false);
        return;
    }

    workspace_show(ws);

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'move [window|container] [to] [absolute] position <px> [px] <px> [px]
 *
 */
void cmd_move_window_to_position(I3_CMD, char *method, char *cx, char *cy) {

    int x = atoi(cx);
    int y = atoi(cy);

    if (!con_is_floating(focused)) {
        ELOG("Cannot change position. The window/container is not floating\n");
        y(map_open);
        ystr("success");
        y(bool, false);
        ystr("error");
        ystr("Cannot change position. The window/container is not floating.");
        y(map_close);
        return;
    }

    if (strcmp(method, "absolute") == 0) {
        focused->parent->rect.x = x;
        focused->parent->rect.y = y;

        DLOG("moving to absolute position %d %d\n", x, y);
        floating_maybe_reassign_ws(focused->parent);
        cmd_output->needs_tree_render = true;
    }

    if (strcmp(method, "position") == 0) {
        Rect newrect = focused->parent->rect;

        DLOG("moving to position %d %d\n", x, y);
        newrect.x = x;
        newrect.y = y;

        floating_reposition(focused->parent, newrect);
    }

    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'move [window|container] [to] [absolute] position center
 *
 */
void cmd_move_window_to_center(I3_CMD, char *method) {

    if (!con_is_floating(focused)) {
        ELOG("Cannot change position. The window/container is not floating\n");
        y(map_open);
        ystr("success");
        y(bool, false);
        ystr("error");
        ystr("Cannot change position. The window/container is not floating.");
        y(map_close);
    }

    if (strcmp(method, "absolute") == 0) {
        Rect *rect = &focused->parent->rect;

        DLOG("moving to absolute center\n");
        rect->x = croot->rect.width/2 - rect->width/2;
        rect->y = croot->rect.height/2 - rect->height/2;

        floating_maybe_reassign_ws(focused->parent);
        cmd_output->needs_tree_render = true;
    }

    if (strcmp(method, "position") == 0) {
        Rect *wsrect = &con_get_workspace(focused)->rect;
        Rect newrect = focused->parent->rect;

        DLOG("moving to center\n");
        newrect.x = wsrect->width/2 - newrect.width/2;
        newrect.y = wsrect->height/2 - newrect.height/2;

        floating_reposition(focused->parent, newrect);
    }

    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'move scratchpad'.
 *
 */
void cmd_move_scratchpad(I3_CMD) {
    DLOG("should move window to scratchpad\n");
    owindow *current;

    HANDLE_EMPTY_MATCH;

    TAILQ_FOREACH(current, &owindows, owindows) {
        DLOG("matching: %p / %s\n", current->con, current->con->name);
        scratchpad_move(current->con);
    }

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'scratchpad show'.
 *
 */
void cmd_scratchpad_show(I3_CMD) {
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

    cmd_output->needs_tree_render = true;
    // XXX: default reply for now, make this a better reply
    ysuccess(true);
}

/*
 * Implementation of 'rename workspace <name> to <name>'
 *
 */
void cmd_rename_workspace(I3_CMD, char *old_name, char *new_name) {
    LOG("Renaming workspace \"%s\" to \"%s\"\n", old_name, new_name);

    Con *output, *workspace = NULL;
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes)
        GREP_FIRST(workspace, output_get_content(output),
            !strcasecmp(child->name, old_name));

    if (!workspace) {
        // TODO: we should include the old workspace name here and use yajl for
        // generating the reply.
        y(map_open);
        ystr("success");
        y(bool, false);
        ystr("error");
        // TODO: better error message
        ystr("Old workspace not found");
        y(map_close);
        return;
    }

    Con *check_dest = NULL;
    TAILQ_FOREACH(output, &(croot->nodes_head), nodes)
        GREP_FIRST(check_dest, output_get_content(output),
            !strcasecmp(child->name, new_name));

    if (check_dest != NULL) {
        // TODO: we should include the new workspace name here and use yajl for
        // generating the reply.
        y(map_open);
        ystr("success");
        y(bool, false);
        ystr("error");
        // TODO: better error message
        ystr("New workspace already exists");
        y(map_close);
        return;
    }

    /* Change the name and try to parse it as a number. */
    FREE(workspace->name);
    workspace->name = sstrdup(new_name);
    char *endptr = NULL;
    long parsed_num = strtol(new_name, &endptr, 10);
    if (parsed_num == LONG_MIN ||
        parsed_num == LONG_MAX ||
        parsed_num < 0 ||
        endptr == new_name)
        workspace->num = -1;
    else workspace->num = parsed_num;
    LOG("num = %d\n", workspace->num);

    /* By re-attaching, the sort order will be correct afterwards. */
    Con *previously_focused = focused;
    Con *parent = workspace->parent;
    con_detach(workspace);
    con_attach(workspace, parent, false);
    /* Restore the previous focus since con_attach messes with the focus. */
    con_focus(previously_focused);

    cmd_output->needs_tree_render = true;
    ysuccess(true);

    ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"rename\"}");
}
