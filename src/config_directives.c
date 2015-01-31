#undef I3__FILE__
#define I3__FILE__ "config_directives.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * config_directives.c: all config storing functions (see config_parser.c)
 *
 */
#include <float.h>
#include <stdarg.h>

#include "all.h"

/*******************************************************************************
 * Criteria functions.
 ******************************************************************************/

static int criteria_next_state;

/*
 * Initializes the specified 'Match' data structure and the initial state of
 * commands.c for matching target windows of a command.
 *
 */
CFGFUN(criteria_init, int _state) {
    criteria_next_state = _state;

    DLOG("Initializing criteria, current_match = %p, state = %d\n", current_match, _state);
    match_init(current_match);
}

CFGFUN(criteria_pop_state) {
    result->next_state = criteria_next_state;
}

/*
 * Interprets a ctype=cvalue pair and adds it to the current match
 * specification.
 *
 */
CFGFUN(criteria_add, const char *ctype, const char *cvalue) {
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
        current_match->window_role = regex_new(cvalue);
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
            current_match->con_id = (Con *)parsed;
            DLOG("id as int = %p\n", current_match->con_id);
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
            DLOG("window id as int = %d\n", current_match->id);
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

/* TODO: refactor the above criteria code into a single file (with src/commands.c). */

/*******************************************************************************
 * Utility functions
 ******************************************************************************/

static bool eval_boolstr(const char *str) {
    return (strcasecmp(str, "1") == 0 ||
            strcasecmp(str, "yes") == 0 ||
            strcasecmp(str, "true") == 0 ||
            strcasecmp(str, "on") == 0 ||
            strcasecmp(str, "enable") == 0 ||
            strcasecmp(str, "active") == 0);
}

/*
 * A utility function to convert a string of modifiers to the corresponding bit
 * mask.
 */
uint32_t modifiers_from_str(const char *str) {
    /* It might be better to use strtok() here, but the simpler strstr() should
     * do for now. */
    uint32_t result = 0;
    if (str == NULL)
        return result;
    if (strstr(str, "Mod1") != NULL)
        result |= BIND_MOD1;
    if (strstr(str, "Mod2") != NULL)
        result |= BIND_MOD2;
    if (strstr(str, "Mod3") != NULL)
        result |= BIND_MOD3;
    if (strstr(str, "Mod4") != NULL)
        result |= BIND_MOD4;
    if (strstr(str, "Mod5") != NULL)
        result |= BIND_MOD5;
    if (strstr(str, "Control") != NULL ||
        strstr(str, "Ctrl") != NULL)
        result |= BIND_CONTROL;
    if (strstr(str, "Shift") != NULL)
        result |= BIND_SHIFT;
    if (strstr(str, "Mode_switch") != NULL)
        result |= BIND_MODE_SWITCH;
    return result;
}

static char *font_pattern;

CFGFUN(font, const char *font) {
    config.font = load_font(font, true);
    set_font(&config.font);

    /* Save the font pattern for using it as bar font later on */
    FREE(font_pattern);
    font_pattern = sstrdup(font);
}

CFGFUN(binding, const char *bindtype, const char *modifiers, const char *key, const char *release, const char *whole_window, const char *command) {
    configure_binding(bindtype, modifiers, key, release, whole_window, command, DEFAULT_BINDING_MODE);
}

/*******************************************************************************
 * Mode handling
 ******************************************************************************/

static char *current_mode;

CFGFUN(mode_binding, const char *bindtype, const char *modifiers, const char *key, const char *release, const char *whole_window, const char *command) {
    configure_binding(bindtype, modifiers, key, release, whole_window, command, current_mode);
}

CFGFUN(enter_mode, const char *modename) {
    if (strcasecmp(modename, DEFAULT_BINDING_MODE) == 0) {
        ELOG("You cannot use the name %s for your mode\n", DEFAULT_BINDING_MODE);
        exit(1);
    }
    DLOG("\t now in mode %s\n", modename);
    FREE(current_mode);
    current_mode = sstrdup(modename);
}

CFGFUN(exec, const char *exectype, const char *no_startup_id, const char *command) {
    struct Autostart *new = smalloc(sizeof(struct Autostart));
    new->command = sstrdup(command);
    new->no_startup_id = (no_startup_id != NULL);
    if (strcmp(exectype, "exec") == 0) {
        TAILQ_INSERT_TAIL(&autostarts, new, autostarts);
    } else {
        TAILQ_INSERT_TAIL(&autostarts_always, new, autostarts_always);
    }
}

CFGFUN(for_window, const char *command) {
    if (match_is_empty(current_match)) {
        ELOG("Match is empty, ignoring this for_window statement\n");
        return;
    }
    DLOG("\t should execute command %s for the criteria mentioned above\n", command);
    Assignment *assignment = scalloc(sizeof(Assignment));
    assignment->type = A_COMMAND;
    match_copy(&(assignment->match), current_match);
    assignment->dest.command = sstrdup(command);
    TAILQ_INSERT_TAIL(&assignments, assignment, assignments);
}

CFGFUN(floating_minimum_size, const long width, const long height) {
    config.floating_minimum_width = width;
    config.floating_minimum_height = height;
}

CFGFUN(floating_maximum_size, const long width, const long height) {
    config.floating_maximum_width = width;
    config.floating_maximum_height = height;
}

CFGFUN(floating_modifier, const char *modifiers) {
    config.floating_modifier = modifiers_from_str(modifiers);
}

CFGFUN(default_orientation, const char *orientation) {
    if (strcmp(orientation, "horizontal") == 0)
        config.default_orientation = HORIZ;
    else if (strcmp(orientation, "vertical") == 0)
        config.default_orientation = VERT;
    else
        config.default_orientation = NO_ORIENTATION;
}

CFGFUN(workspace_layout, const char *layout) {
    if (strcmp(layout, "default") == 0)
        config.default_layout = L_DEFAULT;
    else if (strcmp(layout, "stacking") == 0 ||
             strcmp(layout, "stacked") == 0)
        config.default_layout = L_STACKED;
    else
        config.default_layout = L_TABBED;
}

CFGFUN(new_window, const char *windowtype, const char *border, const long width) {
    int border_style;
    int border_width;

    if (strcmp(border, "1pixel") == 0) {
        border_style = BS_PIXEL;
        border_width = 1;
    } else if (strcmp(border, "none") == 0) {
        border_style = BS_NONE;
        border_width = 0;
    } else if (strcmp(border, "pixel") == 0) {
        border_style = BS_PIXEL;
        border_width = width;
    } else {
        border_style = BS_NORMAL;
        border_width = width;
    }

    if (strcmp(windowtype, "new_window") == 0) {
        DLOG("default tiled border style = %d and border width = %d (%d physical px)\n",
             border_style, border_width, logical_px(border_width));
        config.default_border = border_style;
        config.default_border_width = logical_px(border_width);
    } else {
        DLOG("default floating border style = %d and border width = %d (%d physical px)\n",
             border_style, border_width, logical_px(border_width));
        config.default_floating_border = border_style;
        config.default_floating_border_width = logical_px(border_width);
    }
}

CFGFUN(hide_edge_borders, const char *borders) {
    if (strcmp(borders, "vertical") == 0)
        config.hide_edge_borders = ADJ_LEFT_SCREEN_EDGE | ADJ_RIGHT_SCREEN_EDGE;
    else if (strcmp(borders, "horizontal") == 0)
        config.hide_edge_borders = ADJ_UPPER_SCREEN_EDGE | ADJ_LOWER_SCREEN_EDGE;
    else if (strcmp(borders, "both") == 0)
        config.hide_edge_borders = ADJ_LEFT_SCREEN_EDGE | ADJ_RIGHT_SCREEN_EDGE | ADJ_UPPER_SCREEN_EDGE | ADJ_LOWER_SCREEN_EDGE;
    else if (strcmp(borders, "none") == 0)
        config.hide_edge_borders = ADJ_NONE;
    else if (eval_boolstr(borders))
        config.hide_edge_borders = ADJ_LEFT_SCREEN_EDGE | ADJ_RIGHT_SCREEN_EDGE;
    else
        config.hide_edge_borders = ADJ_NONE;
}

CFGFUN(focus_follows_mouse, const char *value) {
    config.disable_focus_follows_mouse = !eval_boolstr(value);
}

CFGFUN(mouse_warping, const char *value) {
    if (strcmp(value, "none") == 0)
        config.mouse_warping = POINTER_WARPING_NONE;
    else if (strcmp(value, "output") == 0)
        config.mouse_warping = POINTER_WARPING_OUTPUT;
}

CFGFUN(force_xinerama, const char *value) {
    config.force_xinerama = eval_boolstr(value);
}

CFGFUN(force_focus_wrapping, const char *value) {
    config.force_focus_wrapping = eval_boolstr(value);
}

CFGFUN(workspace_back_and_forth, const char *value) {
    config.workspace_auto_back_and_forth = eval_boolstr(value);
}

CFGFUN(fake_outputs, const char *outputs) {
    config.fake_outputs = sstrdup(outputs);
}

CFGFUN(force_display_urgency_hint, const long duration_ms) {
    config.workspace_urgency_timer = duration_ms / 1000.0;
}

CFGFUN(workspace, const char *workspace, const char *output) {
    DLOG("Assigning workspace \"%s\" to output \"%s\"\n", workspace, output);
    /* Check for earlier assignments of the same workspace so that we
     * don’t have assignments of a single workspace to different
     * outputs */
    struct Workspace_Assignment *assignment;
    bool duplicate = false;
    TAILQ_FOREACH(assignment, &ws_assignments, ws_assignments) {
        if (strcasecmp(assignment->name, workspace) == 0) {
            ELOG("You have a duplicate workspace assignment for workspace \"%s\"\n",
                 workspace);
            assignment->output = sstrdup(output);
            duplicate = true;
        }
    }
    if (!duplicate) {
        assignment = scalloc(sizeof(struct Workspace_Assignment));
        assignment->name = sstrdup(workspace);
        assignment->output = sstrdup(output);
        TAILQ_INSERT_TAIL(&ws_assignments, assignment, ws_assignments);
    }
}

CFGFUN(ipc_socket, const char *path) {
    config.ipc_socket_path = sstrdup(path);
}

CFGFUN(restart_state, const char *path) {
    config.restart_state_path = sstrdup(path);
}

CFGFUN(popup_during_fullscreen, const char *value) {
    if (strcmp(value, "ignore") == 0) {
        config.popup_during_fullscreen = PDF_IGNORE;
    } else if (strcmp(value, "leave_fullscreen") == 0) {
        config.popup_during_fullscreen = PDF_LEAVE_FULLSCREEN;
    } else {
        config.popup_during_fullscreen = PDF_SMART;
    }
}

CFGFUN(color_single, const char *colorclass, const char *color) {
    /* used for client.background only currently */
    config.client.background = get_colorpixel(color);
}

CFGFUN(color, const char *colorclass, const char *border, const char *background, const char *text, const char *indicator) {
#define APPLY_COLORS(classname)                                                \
    do {                                                                       \
        if (strcmp(colorclass, "client." #classname) == 0) {                   \
            config.client.classname.border = get_colorpixel(border);           \
            config.client.classname.background = get_colorpixel(background);   \
            config.client.classname.text = get_colorpixel(text);               \
            if (indicator != NULL) {                                           \
                config.client.classname.indicator = get_colorpixel(indicator); \
            }                                                                  \
        }                                                                      \
    } while (0)

    APPLY_COLORS(focused_inactive);
    APPLY_COLORS(focused);
    APPLY_COLORS(unfocused);
    APPLY_COLORS(urgent);
    APPLY_COLORS(placeholder);

#undef APPLY_COLORS
}

CFGFUN(assign, const char *workspace) {
    if (match_is_empty(current_match)) {
        ELOG("Match is empty, ignoring this assignment\n");
        return;
    }
    DLOG("new assignment, using above criteria, to workspace %s\n", workspace);
    Assignment *assignment = scalloc(sizeof(Assignment));
    match_copy(&(assignment->match), current_match);
    assignment->type = A_TO_WORKSPACE;
    assignment->dest.workspace = sstrdup(workspace);
    TAILQ_INSERT_TAIL(&assignments, assignment, assignments);
}

/*******************************************************************************
 * Bar configuration (i3bar)
 ******************************************************************************/

static Barconfig current_bar;

CFGFUN(bar_font, const char *font) {
    FREE(current_bar.font);
    current_bar.font = sstrdup(font);
}

CFGFUN(bar_mode, const char *mode) {
    current_bar.mode = (strcmp(mode, "dock") == 0 ? M_DOCK : (strcmp(mode, "hide") == 0 ? M_HIDE : M_INVISIBLE));
}

CFGFUN(bar_hidden_state, const char *hidden_state) {
    current_bar.hidden_state = (strcmp(hidden_state, "hide") == 0 ? S_HIDE : S_SHOW);
}

CFGFUN(bar_id, const char *bar_id) {
    current_bar.id = sstrdup(bar_id);
}

CFGFUN(bar_output, const char *output) {
    int new_outputs = current_bar.num_outputs + 1;
    current_bar.outputs = srealloc(current_bar.outputs, sizeof(char *) * new_outputs);
    current_bar.outputs[current_bar.num_outputs] = sstrdup(output);
    current_bar.num_outputs = new_outputs;
}

CFGFUN(bar_verbose, const char *verbose) {
    current_bar.verbose = eval_boolstr(verbose);
}

CFGFUN(bar_modifier, const char *modifier) {
    if (strcmp(modifier, "Mod1") == 0)
        current_bar.modifier = M_MOD1;
    else if (strcmp(modifier, "Mod2") == 0)
        current_bar.modifier = M_MOD2;
    else if (strcmp(modifier, "Mod3") == 0)
        current_bar.modifier = M_MOD3;
    else if (strcmp(modifier, "Mod4") == 0)
        current_bar.modifier = M_MOD4;
    else if (strcmp(modifier, "Mod5") == 0)
        current_bar.modifier = M_MOD5;
    else if (strcmp(modifier, "Control") == 0 ||
             strcmp(modifier, "Ctrl") == 0)
        current_bar.modifier = M_CONTROL;
    else if (strcmp(modifier, "Shift") == 0)
        current_bar.modifier = M_SHIFT;
}

CFGFUN(bar_wheel_up_cmd, const char *command) {
    FREE(current_bar.wheel_up_cmd);
    current_bar.wheel_up_cmd = sstrdup(command);
}

CFGFUN(bar_wheel_down_cmd, const char *command) {
    FREE(current_bar.wheel_down_cmd);
    current_bar.wheel_down_cmd = sstrdup(command);
}

CFGFUN(bar_position, const char *position) {
    current_bar.position = (strcmp(position, "top") == 0 ? P_TOP : P_BOTTOM);
}

CFGFUN(bar_i3bar_command, const char *i3bar_command) {
    FREE(current_bar.i3bar_command);
    current_bar.i3bar_command = sstrdup(i3bar_command);
}

CFGFUN(bar_color, const char *colorclass, const char *border, const char *background, const char *text) {
#define APPLY_COLORS(classname)                                          \
    do {                                                                 \
        if (strcmp(colorclass, #classname) == 0) {                       \
            if (text != NULL) {                                          \
                /* New syntax: border, background, text */               \
                current_bar.colors.classname##_border = sstrdup(border); \
                current_bar.colors.classname##_bg = sstrdup(background); \
                current_bar.colors.classname##_text = sstrdup(text);     \
            } else {                                                     \
                /* Old syntax: text, background */                       \
                current_bar.colors.classname##_bg = sstrdup(background); \
                current_bar.colors.classname##_text = sstrdup(border);   \
            }                                                            \
        }                                                                \
    } while (0)

    APPLY_COLORS(focused_workspace);
    APPLY_COLORS(active_workspace);
    APPLY_COLORS(inactive_workspace);
    APPLY_COLORS(urgent_workspace);

#undef APPLY_COLORS
}

CFGFUN(bar_socket_path, const char *socket_path) {
    FREE(current_bar.socket_path);
    current_bar.socket_path = sstrdup(socket_path);
}

CFGFUN(bar_tray_output, const char *output) {
    FREE(current_bar.tray_output);
    current_bar.tray_output = sstrdup(output);
}

CFGFUN(bar_color_single, const char *colorclass, const char *color) {
    if (strcmp(colorclass, "background") == 0)
        current_bar.colors.background = sstrdup(color);
    else if (strcmp(colorclass, "separator") == 0)
        current_bar.colors.separator = sstrdup(color);
    else
        current_bar.colors.statusline = sstrdup(color);
}

CFGFUN(bar_status_command, const char *command) {
    FREE(current_bar.status_command);
    current_bar.status_command = sstrdup(command);
}

CFGFUN(bar_binding_mode_indicator, const char *value) {
    current_bar.hide_binding_mode_indicator = !eval_boolstr(value);
}

CFGFUN(bar_workspace_buttons, const char *value) {
    current_bar.hide_workspace_buttons = !eval_boolstr(value);
}

CFGFUN(bar_strip_workspace_numbers, const char *value) {
    current_bar.strip_workspace_numbers = eval_boolstr(value);
}

CFGFUN(bar_finish) {
    DLOG("\t new bar configuration finished, saving.\n");
    /* Generate a unique ID for this bar if not already configured */
    if (!current_bar.id)
        sasprintf(&current_bar.id, "bar-%d", config.number_barconfigs);

    config.number_barconfigs++;

    /* If no font was explicitly set, we use the i3 font as default */
    if (!current_bar.font && font_pattern)
        current_bar.font = sstrdup(font_pattern);

    /* Copy the current (static) structure into a dynamically allocated
     * one, then cleanup our static one. */
    Barconfig *bar_config = scalloc(sizeof(Barconfig));
    memcpy(bar_config, &current_bar, sizeof(Barconfig));
    TAILQ_INSERT_TAIL(&barconfigs, bar_config, configs);

    memset(&current_bar, '\0', sizeof(Barconfig));
}
