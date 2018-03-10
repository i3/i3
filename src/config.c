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

#include <xkbcommon/xkbcommon.h>

char *current_configpath = NULL;
char *current_config = NULL;
Config config;
struct modes_head modes;
struct barconfig_head barconfigs = TAILQ_HEAD_INITIALIZER(barconfigs);

/**
 * Ungrabs all keys, to be called before re-grabbing the keys because of a
 * mapping_notify event or a configuration file reload
 *
 */
void ungrab_all_keys(xcb_connection_t *conn) {
    DLOG("Ungrabbing all keys\n");
    xcb_ungrab_key(conn, XCB_GRAB_ANY, root, XCB_BUTTON_MASK_ANY);
}

/*
 * Sends the current bar configuration as an event to all barconfig_update listeners.
 *
 */
void update_barconfig() {
    Barconfig *current;
    TAILQ_FOREACH(current, &barconfigs, configs) {
        ipc_send_barconfig_update_event(current);
    }
}

/*
 * Finds the configuration file to use (either the one specified by
 * override_configpath), the user’s one or the system default) and calls
 * parse_file().
 *
 */
bool parse_configuration(const char *override_configpath, bool use_nagbar) {
    char *path = get_config_path(override_configpath, true);
    if (path == NULL) {
        die("Unable to find the configuration file (looked at "
            "~/.i3/config, $XDG_CONFIG_HOME/i3/config, " SYSCONFDIR "/i3/config and $XDG_CONFIG_DIRS/i3/config)");
    }

    LOG("Parsing configfile %s\n", path);
    FREE(current_configpath);
    current_configpath = path;

    /* initialize default bindings if we're just validating the config file */
    if (!use_nagbar && bindings == NULL) {
        bindings = scalloc(1, sizeof(struct bindings_head));
        TAILQ_INIT(bindings);
    }

    return parse_file(path, use_nagbar);
}

/*
 * (Re-)loads the configuration file (sets useful defaults before).
 *
 */
void load_configuration(xcb_connection_t *conn, const char *override_configpath, bool reload) {
    if (reload) {
        /* If we are currently in a binding mode, we first revert to the
         * default since we have no guarantee that the current mode will even
         * still exist after parsing the config again. See #2228. */
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

        struct Assignment *assign;
        while (!TAILQ_EMPTY(&assignments)) {
            assign = TAILQ_FIRST(&assignments);
            if (assign->type == A_TO_WORKSPACE)
                FREE(assign->dest.workspace);
            else if (assign->type == A_COMMAND)
                FREE(assign->dest.command);
            match_free(&(assign->match));
            TAILQ_REMOVE(&assignments, assign, assignments);
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

        /* Invalidate pixmap caches in case font or colors changed */
        Con *con;
        TAILQ_FOREACH(con, &all_cons, all_cons)
        FREE(con->deco_render_params);

        /* Get rid of the current font */
        free_font();

        free(config.ipc_socket_path);
        free(config.restart_state_path);
        free(config.fake_outputs);
    }

    SLIST_INIT(&modes);

    struct Mode *default_mode = scalloc(1, sizeof(struct Mode));
    default_mode->name = sstrdup("default");
    default_mode->bindings = scalloc(1, sizeof(struct bindings_head));
    TAILQ_INIT(default_mode->bindings);
    SLIST_INSERT_HEAD(&modes, default_mode, modes);

    bindings = default_mode->bindings;

#define REQUIRED_OPTION(name) \
    if (config.name == NULL)  \
        die("You did not specify required configuration option " #name "\n");

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

    parse_configuration(override_configpath, true);

    if (reload) {
        translate_keysyms();
        grab_all_keys(conn);
        regrab_all_buttons(conn);
    }

    if (config.font.type == FONT_TYPE_NONE) {
        ELOG("You did not specify required configuration option \"font\"\n");
        config.font = load_font("fixed", true);
        set_font(&config.font);
    }

    /* Redraw the currently visible decorations on reload, so that
     * the possibly new drawing parameters changed. */
    if (reload) {
        x_deco_recurse(croot);
        xcb_flush(conn);
    }
}
