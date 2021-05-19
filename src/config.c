/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * config.c: Configuration file (calling the parser (src/config_parser.c) with
 *           the correct path, switching key bindings mode).
 *
 */
#include "all.h"
#include "yajl_utils.h"

#include <xkbcommon/xkbcommon.h>

char *current_configpath = NULL;
char *current_config = NULL;
Config config;
struct modes_head modes;
struct barconfig_head barconfigs = TAILQ_HEAD_INITIALIZER(barconfigs);

/*
 * Ungrabs all keys, to be called before re-grabbing the keys because of a
 * mapping_notify event or a configuration file reload
 *
 */
void ungrab_all_keys(xcb_connection_t *conn) {
    DLOG("Ungrabbing all keys\n");
    xcb_ungrab_key(conn, XCB_GRAB_ANY, root, XCB_BUTTON_MASK_ANY);
}

static void color_to_hex(color_t color, char *hex) {
    hex[0] = '#';
    sprintf(hex+1, "%02x", (int) (color.red * 255));
    sprintf(hex+3, "%02x", (int) (color.green * 255));
    sprintf(hex+5, "%02x", (int) (color.blue * 255));
    sprintf(hex+7, "%02x", (int) (color.alpha * 255));
    hex[9] = '\0';
}

static void dump_colortriple(yajl_gen gen, struct Colortriple triple) {
    char str[10];

    y(map_open);
#define ADD(name) \
    ystr(#name); \
    color_to_hex(triple.name, str); \
    ystr(str);

    ADD(border);
    ADD(background);
    ADD(text);
    ADD(indicator);
    ADD(child_border);
#undef ADD
    y(map_close);
}

static void generate_structured_config(void) {
    yajl_gen gen = config.json_gen;

    y(map_open);

    /* Values from Config */
#define CINT(name)  ystr(#name); y(integer, config.name)
#define CBOOL(name) ystr(#name); y(bool, config.name)
#define CSTR(name)  if (config.name != NULL) { ystr(#name); ystr(config.name); }
    CSTR(ipc_socket_path);
    CSTR(restart_state_path);
    CINT(default_border_width);
    CINT(default_floating_border_width);
    CBOOL(disable_focus_follows_mouse);
    CBOOL(force_xinerama);
    CBOOL(disable_randr15);
    CBOOL(workspace_auto_back_and_forth);
    CBOOL(show_marks);
    CINT(floating_maximum_width);
    CINT(floating_maximum_height);
    CINT(floating_minimum_width);
    CINT(floating_minimum_height);
    CSTR(fake_outputs);
#undef CSTR
#undef CINT
#undef CBOOL

    /* Enums and the like from Config */
    if (config.font.pattern != NULL) {
        ystr("font");
        ystr(config.font.pattern);
    }

    ystr("floating_modifier");
    dump_event_state_mask(gen, config.floating_modifier);

    ystr("default_layout");
    switch (config.default_layout) {
        case L_DEFAULT:
            ystr("default");
            break;
        case L_STACKED:
            ystr("stacked");
            break;
        case L_TABBED:
            ystr("tabbed");
            break;
        case L_DOCKAREA:
            ystr("dockarea");
            break;
        case L_OUTPUT:
            ystr("output");
            break;
        case L_SPLITV:
            ystr("splitv");
            break;
        case L_SPLITH:
            ystr("splith");
            break;
    }

    ystr("default_orientation");
    switch (config.default_orientation) {
        case HORIZ:
            ystr("horizontal");
            break;
        case VERT:
            ystr("vertical");
            break;
        case NO_ORIENTATION:
            ystr("none");
            break;
    }

    ystr("mouse_warping");
    switch (config.mouse_warping) {
        case POINTER_WARPING_OUTPUT:
            ystr("output");
            break;
        case POINTER_WARPING_NONE:
            ystr("none");
            break;
    }

    ystr("hide_edge_borders");
    switch (config.hide_edge_borders) {
        case HEBM_NONE:
            ystr("none");
            break;
        case HEBM_VERTICAL:
            ystr("vertical");
            break;
        case HEBM_HORIZONTAL:
            ystr("horizontal");
            break;
        case HEBM_BOTH:
            ystr("both");
            break;
        case HEBM_SMART:
            ystr("smart");
            break;
    }

    ystr("focus_wrapping");
    switch (config.focus_wrapping) {
        case FOCUS_WRAPPING_OFF:
            ystr("off");
            break;
        case FOCUS_WRAPPING_ON:
            ystr("on");
            break;
        case FOCUS_WRAPPING_FORCE:
            ystr("force");
            break;
        case FOCUS_WRAPPING_WORKSPACE:
            ystr("workspace");
            break;
    }

    ystr("workspace_urgency_timer");
    y(double, config.workspace_urgency_timer);

    ystr("focus_on_window_activation");
    switch (config.focus_on_window_activation) {
        case FOWA_SMART:
            ystr("smart");
            break;
        case FOWA_URGENT:
            ystr("urgent");
            break;
        case FOWA_FOCUS:
            ystr("focus");
            break;
        case FOWA_NONE:
            ystr("none");
            break;
    }

    ystr("title_align");
    switch (config.title_align) {
        case ALIGN_LEFT:
            ystr("left");
            break;
        case ALIGN_CENTER:
            ystr("center");
            break;
        case ALIGN_RIGHT:
            ystr("right");
            break;
    }

    ystr("default_border");
    switch (config.default_border) {
        case BS_NORMAL:
            ystr("normal");
            break;
        case BS_NONE:
            ystr("none");
            break;
        case BS_PIXEL:
            ystr("pixel");
            break;
    }
    ystr("default_floating_border");
    switch (config.default_floating_border) {
        case BS_NORMAL:
            ystr("normal");
            break;
        case BS_NONE:
            ystr("none");
            break;
        case BS_PIXEL:
            ystr("pixel");
            break;
    }

/* 'element' is the element of Config (client, bar) and 'class' is the color
 * class to be saved */
#define ADD(element, class) \
    ystr(#class); \
    dump_colortriple(gen, config.element.class);

    ystr("colors_client");
    y(map_open);

    char client_bg[10];
    color_to_hex(config.client.background, client_bg);
    ystr("background");
    ystr(client_bg);

    ADD(client, focused);
    ADD(client, focused_inactive);
    ADD(client, unfocused);
    ADD(client, urgent);
    ADD(client, placeholder);

    y(map_close); /* colors_client */

    ystr("colors_bar");
    y(map_open);

    ADD(bar, focused);
    ADD(bar, unfocused);
    ADD(bar, urgent);

    y(map_close);
#undef ADD

    ystr("popup_during_fullscreen");
    switch (config.popup_during_fullscreen) {
        case PDF_SMART:
            ystr("smart");
            break;
        case PDF_LEAVE_FULLSCREEN:
            ystr("leave_fullscreen");
            break;
        case PDF_IGNORE:
            ystr("ignore");
            break;
    }
    /* exec */
    ystr("exec");
    y(array_open);

    struct Autostart *autostart;
    TAILQ_FOREACH (autostart, &autostarts, autostarts) {
        y(map_open);

        ystr("command");
        ystr(autostart->command);

        ystr("no-startup-id");
        y(bool, (int) autostart->no_startup_id);

        y(map_close);
    }
    y(array_close);

    /* exec_always */
    ystr("exec_always");
    y(array_open);

    TAILQ_FOREACH (autostart, &autostarts_always, autostarts_always) {
        y(map_open);

        ystr("command");
        ystr(autostart->command);

        ystr("no-startup-id");
        y(bool, (int) autostart->no_startup_id);

        y(map_close);
    }
    y(array_close);

    /* Assignments */
    ystr("assignments");
    y(array_open);

    Assignment *assignment;
    TAILQ_FOREACH (assignment, &assignments, assignments) {
        y(map_open);
        ystr("type");
        switch (assignment->type) {
            case A_COMMAND:
                ystr("command");
                break;
            case A_TO_WORKSPACE:
                ystr("workspace");
                break;
            case A_TO_WORKSPACE_NUMBER:
                ystr("workspace_number");
                break;
            case A_TO_OUTPUT:
                ystr("output");
                break;
            case A_NO_FOCUS:
                ystr("no_focus");
                break;
            case A_ANY:
                break;
        }

        if (assignment->type & (A_COMMAND | A_TO_WORKSPACE |
                                A_TO_WORKSPACE_NUMBER | A_TO_OUTPUT)) {
            ystr("value");
            switch (assignment->type) {
                case A_COMMAND:
                    ystr(assignment->dest.command);
                    break;
                case A_TO_WORKSPACE:
                case A_TO_WORKSPACE_NUMBER:
                    ystr(assignment->dest.workspace);
                    break;
                case A_TO_OUTPUT:
                    ystr(assignment->dest.output);
                    break;
                default:
                    /* to make the compiler happy */
                    break;
            }
        }

        /* The Match (criteria) */
        ystr("criteria");
        y(map_open);
#define ADD(name) \
    if (assignment->match.name != NULL) { \
        ystr(#name); \
        ystr(assignment->match.name->pattern); \
    }
        ADD(title);
        ADD(application);
        ADD(class);
        ADD(instance);
        ADD(mark);
        ADD(window_role);
        ADD(workspace);
        ADD(machine);
#undef ADD

        if (assignment->match.window_type != UINT32_MAX) {
            ystr("window_type");
            if (assignment->match.window_type == A__NET_WM_WINDOW_TYPE_NORMAL) {
                ystr("normal");
            } else if (assignment->match.window_type == A__NET_WM_WINDOW_TYPE_DOCK) {
                ystr("dock");
            } else if (assignment->match.window_type == A__NET_WM_WINDOW_TYPE_DIALOG) {
                ystr("dialog");
            } else if (assignment->match.window_type == A__NET_WM_WINDOW_TYPE_UTILITY) {
                ystr("utility");
            } else if (assignment->match.window_type == A__NET_WM_WINDOW_TYPE_TOOLBAR) {
                ystr("toolbar");
            } else if (assignment->match.window_type == A__NET_WM_WINDOW_TYPE_SPLASH) {
                ystr("splash");
            } else if (assignment->match.window_type == A__NET_WM_WINDOW_TYPE_MENU) {
                ystr("menu");
            } else if (assignment->match.window_type == A__NET_WM_WINDOW_TYPE_DROPDOWN_MENU) {
                ystr("dropdown_menu");
            } else if (assignment->match.window_type == A__NET_WM_WINDOW_TYPE_POPUP_MENU) {
                ystr("popup_menu");
            } else if (assignment->match.window_type == A__NET_WM_WINDOW_TYPE_TOOLTIP) {
                ystr("tooltip");
            } else if (assignment->match.window_type == A__NET_WM_WINDOW_TYPE_NOTIFICATION) {
                ystr("notification");
            } else {
                ELOG("Illegal value of window type: %d. This is probably a bug in i3\n",
                     assignment->match.window_type);
                y(null);
            }
        }

        if (assignment->match.urgent != U_DONTCHECK) {
            ystr("urgent");
            switch (assignment->match.urgent) {
                case U_LATEST:
                    ystr("latest");
                    break;
                case U_OLDEST:
                    ystr("oldest");
                    break;
                case U_DONTCHECK:
                    break;
            }
        }

        if (assignment->match.window_mode != WM_ANY) {
            ystr("window_mode");
            switch (assignment->match.window_mode) {
               case  WM_TILING_AUTO:
                    ystr("tiling_auto");
                    break;
                case WM_TILING_USER:
                    ystr("tiling_user");
                    break;
                case WM_TILING:
                    ystr("tiling");
                    break;
                case WM_FLOATING_AUTO:
                    ystr("floating_auto");
                    break;
                case WM_FLOATING_USER:
                    ystr("floating_user");
                    break;
                case WM_FLOATING:
                    ystr("floating");
                    break;
                case WM_ANY:
                    break;
            }
        }

        if (assignment->match.con_id != NULL) {
            ystr("con_id");
            y(integer, (long) assignment->match.con_id);
        }

        if (assignment->match.id != 0) {
            ystr("id");
            y(integer, assignment->match.id);
        }

        y(map_close); /* Match */
        y(map_close); /* Assignment */
    }
    y(array_close); /* Assignments */
    y(map_close);
}

static void free_configuration(void) {
    assert(conn != NULL);

    /* If we are currently in a binding mode, we first revert to the default
     * since we have no guarantee that the current mode will even still exist
     * after parsing the config again. See #2228. */
    switch_mode("default");

    /* First ungrab the keys */
    ungrab_all_keys(conn);

    struct Mode *mode;
    while (!SLIST_EMPTY(&modes)) {
        mode = SLIST_FIRST(&modes);
        FREE(mode->name);

        /* Clear the old binding list */
        while (!TAILQ_EMPTY(mode->bindings)) {
            Binding *bind = TAILQ_FIRST(mode->bindings);
            TAILQ_REMOVE(mode->bindings, bind, bindings);
            binding_free(bind);
        }
        FREE(mode->bindings);

        SLIST_REMOVE(&modes, mode, Mode, modes);
        FREE(mode);
    }

    while (!TAILQ_EMPTY(&assignments)) {
        struct Assignment *assign = TAILQ_FIRST(&assignments);
        if (assign->type == A_TO_WORKSPACE || assign->type == A_TO_WORKSPACE_NUMBER)
            FREE(assign->dest.workspace);
        else if (assign->type == A_COMMAND)
            FREE(assign->dest.command);
        else if (assign->type == A_TO_OUTPUT)
            FREE(assign->dest.output);
        match_free(&(assign->match));
        TAILQ_REMOVE(&assignments, assign, assignments);
        FREE(assign);
    }

    while (!TAILQ_EMPTY(&ws_assignments)) {
        struct Workspace_Assignment *assign = TAILQ_FIRST(&ws_assignments);
        FREE(assign->name);
        FREE(assign->output);
        TAILQ_REMOVE(&ws_assignments, assign, ws_assignments);
        FREE(assign);
    }

    /* Clear bar configs */
    Barconfig *barconfig;
    while (!TAILQ_EMPTY(&barconfigs)) {
        barconfig = TAILQ_FIRST(&barconfigs);
        FREE(barconfig->id);
        for (int c = 0; c < barconfig->num_outputs; c++)
            free(barconfig->outputs[c]);

        while (!TAILQ_EMPTY(&(barconfig->bar_bindings))) {
            struct Barbinding *binding = TAILQ_FIRST(&(barconfig->bar_bindings));
            FREE(binding->command);
            TAILQ_REMOVE(&(barconfig->bar_bindings), binding, bindings);
            FREE(binding);
        }

        while (!TAILQ_EMPTY(&(barconfig->tray_outputs))) {
            struct tray_output_t *tray_output = TAILQ_FIRST(&(barconfig->tray_outputs));
            FREE(tray_output->output);
            TAILQ_REMOVE(&(barconfig->tray_outputs), tray_output, tray_outputs);
            FREE(tray_output);
        }

        FREE(barconfig->outputs);
        FREE(barconfig->socket_path);
        FREE(barconfig->status_command);
        FREE(barconfig->i3bar_command);
        FREE(barconfig->font);
        FREE(barconfig->colors.background);
        FREE(barconfig->colors.statusline);
        FREE(barconfig->colors.separator);
        FREE(barconfig->colors.focused_background);
        FREE(barconfig->colors.focused_statusline);
        FREE(barconfig->colors.focused_separator);
        FREE(barconfig->colors.focused_workspace_border);
        FREE(barconfig->colors.focused_workspace_bg);
        FREE(barconfig->colors.focused_workspace_text);
        FREE(barconfig->colors.active_workspace_border);
        FREE(barconfig->colors.active_workspace_bg);
        FREE(barconfig->colors.active_workspace_text);
        FREE(barconfig->colors.inactive_workspace_border);
        FREE(barconfig->colors.inactive_workspace_bg);
        FREE(barconfig->colors.inactive_workspace_text);
        FREE(barconfig->colors.urgent_workspace_border);
        FREE(barconfig->colors.urgent_workspace_bg);
        FREE(barconfig->colors.urgent_workspace_text);
        FREE(barconfig->colors.binding_mode_border);
        FREE(barconfig->colors.binding_mode_bg);
        FREE(barconfig->colors.binding_mode_text);
        TAILQ_REMOVE(&barconfigs, barconfig, configs);
        FREE(barconfig);
    }

    Con *con;
    TAILQ_FOREACH (con, &all_cons, all_cons) {
        /* Assignments changed, previously ran assignments are invalid. */
        if (con->window) {
            con->window->nr_assignments = 0;
            FREE(con->window->ran_assignments);
        }
        /* Invalidate pixmap caches in case font or colors changed. */
        FREE(con->deco_render_params);
    }

    /* Get rid of the current font */
    free_font();

    free(config.ipc_socket_path);
    free(config.restart_state_path);
    free(config.fake_outputs);

    /* Free the structured config */
    yajl_gen_free(config.json_gen);
    config.json_len = 0;
}

/*
 * (Re-)loads the configuration file (sets useful defaults before).
 *
 * If you specify override_configpath, only this path is used to look for a
 * configuration file.
 *
 * load_type specifies the type of loading: C_VALIDATE is used to only verify
 * the correctness of the config file (used with the flag -C). C_LOAD will load
 * the config for normal use and display errors in the nagbar. C_RELOAD will
 * also clear the previous config.
 *
 */
bool load_configuration(const char *override_configpath, config_load_t load_type) {
    if (load_type == C_RELOAD) {
        free_configuration();
    }

    SLIST_INIT(&modes);

    struct Mode *default_mode = scalloc(1, sizeof(struct Mode));
    default_mode->name = sstrdup("default");
    default_mode->bindings = scalloc(1, sizeof(struct bindings_head));
    TAILQ_INIT(default_mode->bindings);
    SLIST_INSERT_HEAD(&modes, default_mode, modes);

    bindings = default_mode->bindings;
    current_binding_mode = default_mode->name;

    /* Clear the old config or initialize the data structure */
    memset(&config, 0, sizeof(config));

    /* Initialize default colors */
#define INIT_COLOR(x, cborder, cbackground, ctext, cindicator) \
    do {                                                       \
        x.border = draw_util_hex_to_color(cborder);            \
        x.background = draw_util_hex_to_color(cbackground);    \
        x.text = draw_util_hex_to_color(ctext);                \
        x.indicator = draw_util_hex_to_color(cindicator);      \
        x.child_border = draw_util_hex_to_color(cbackground);  \
    } while (0)

    config.client.background = draw_util_hex_to_color("#000000");
    INIT_COLOR(config.client.focused, "#4c7899", "#285577", "#ffffff", "#2e9ef4");
    INIT_COLOR(config.client.focused_inactive, "#333333", "#5f676a", "#ffffff", "#484e50");
    INIT_COLOR(config.client.unfocused, "#333333", "#222222", "#888888", "#292d2e");
    INIT_COLOR(config.client.urgent, "#2f343a", "#900000", "#ffffff", "#900000");

    /* border and indicator color are ignored for placeholder contents */
    INIT_COLOR(config.client.placeholder, "#000000", "#0c0c0c", "#ffffff", "#000000");

    /* the last argument (indicator color) is ignored for bar colors */
    INIT_COLOR(config.bar.focused, "#4c7899", "#285577", "#ffffff", "#000000");
    INIT_COLOR(config.bar.unfocused, "#333333", "#222222", "#888888", "#000000");
    INIT_COLOR(config.bar.urgent, "#2f343a", "#900000", "#ffffff", "#000000");

    config.show_marks = true;

    config.default_border = BS_NORMAL;
    config.default_floating_border = BS_NORMAL;
    config.default_border_width = logical_px(2);
    config.default_floating_border_width = logical_px(2);
    /* Set default_orientation to NO_ORIENTATION for auto orientation. */
    config.default_orientation = NO_ORIENTATION;

    /* Set default urgency reset delay to 500ms */
    if (config.workspace_urgency_timer == 0)
        config.workspace_urgency_timer = 0.5;

    config.focus_wrapping = FOCUS_WRAPPING_ON;

    FREE(current_configpath);
    current_configpath = get_config_path(override_configpath, true);
    if (current_configpath == NULL) {
        die("Unable to find the configuration file (looked at "
            "$XDG_CONFIG_HOME/i3/config, ~/.i3/config, $XDG_CONFIG_DIRS/i3/config "
            "and " SYSCONFDIR "/i3/config)");
    }
    LOG("Parsing configfile %s\n", current_configpath);
    const bool result = parse_file(current_configpath, load_type != C_VALIDATE);

    if (config.font.type == FONT_TYPE_NONE && load_type != C_VALIDATE) {
        ELOG("You did not specify required configuration option \"font\"\n");
        config.font = load_font("fixed", true);
        set_font(&config.font);
    }

    if (load_type == C_RELOAD) {
        translate_keysyms();
        grab_all_keys(conn);
        regrab_all_buttons(conn);

        /* Redraw the currently visible decorations on reload, so that the
         * possibly new drawing parameters changed. */
        x_deco_recurse(croot);
        xcb_flush(conn);
    }

    /* Generate the structured configuration information */
    config.json_gen = ygenalloc();
    generate_structured_config();
    yajl_gen_get_buf(config.json_gen, (const unsigned char **) &(config.json), &(config.json_len));

    return result;
}
