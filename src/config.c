#undef I3__FILE__
#define I3__FILE__ "config.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * config.c: Configuration file (calling the parser (src/config_parser.c) with
 *           the correct path, switching key bindings mode).
 *
 */
#include "all.h"

/* We need Xlib for XStringToKeysym */
#include <X11/Xlib.h>

char *current_configpath = NULL;
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
    TAILQ_FOREACH (current, &barconfigs, configs) {
        ipc_send_barconfig_update_event(current);
    }
}

/*
 * Get the path of the first configuration file found. If override_configpath
 * is specified, that path is returned and saved for further calls. Otherwise,
 * checks the home directory first, then the system directory first, always
 * taking into account the XDG Base Directory Specification ($XDG_CONFIG_HOME,
 * $XDG_CONFIG_DIRS)
 *
 */
static char *get_config_path(const char *override_configpath) {
    char *xdg_config_home, *xdg_config_dirs, *config_path;

    static const char *saved_configpath = NULL;

    if (override_configpath != NULL) {
        saved_configpath = override_configpath;
        return sstrdup(saved_configpath);
    }

    if (saved_configpath != NULL)
        return sstrdup(saved_configpath);

    /* 1: check the traditional path under the home directory */
    config_path = resolve_tilde("~/.i3/config");
    if (path_exists(config_path))
        return config_path;
    free(config_path);

    /* 2: check for $XDG_CONFIG_HOME/i3/config */
    if ((xdg_config_home = getenv("XDG_CONFIG_HOME")) == NULL)
        xdg_config_home = "~/.config";

    xdg_config_home = resolve_tilde(xdg_config_home);
    sasprintf(&config_path, "%s/i3/config", xdg_config_home);
    free(xdg_config_home);

    if (path_exists(config_path))
        return config_path;
    free(config_path);

    /* 3: check the traditional path under /etc */
    config_path = SYSCONFDIR "/i3/config";
    if (path_exists(config_path))
        return sstrdup(config_path);

    /* 4: check for $XDG_CONFIG_DIRS/i3/config */
    if ((xdg_config_dirs = getenv("XDG_CONFIG_DIRS")) == NULL)
        xdg_config_dirs = "/etc/xdg";

    char *buf = sstrdup(xdg_config_dirs);
    char *tok = strtok(buf, ":");
    while (tok != NULL) {
        tok = resolve_tilde(tok);
        sasprintf(&config_path, "%s/i3/config", tok);
        free(tok);
        if (path_exists(config_path)) {
            free(buf);
            return config_path;
        }
        free(config_path);
        tok = strtok(NULL, ":");
    }
    free(buf);

    die("Unable to find the configuration file (looked at "
        "~/.i3/config, $XDG_CONFIG_HOME/i3/config, " SYSCONFDIR "/i3/config and $XDG_CONFIG_DIRS/i3/config)");
}

/*
 * Finds the configuration file to use (either the one specified by
 * override_configpath), the user’s one or the system default) and calls
 * parse_file().
 *
 */
static void parse_configuration(const char *override_configpath) {
    char *path = get_config_path(override_configpath);
    LOG("Parsing configfile %s\n", path);
    FREE(current_configpath);
    current_configpath = path;
    parse_file(path);
}

/*
 * (Re-)loads the configuration file (sets useful defaults before).
 *
 */
void load_configuration(xcb_connection_t *conn, const char *override_configpath, bool reload) {
    if (reload) {
        /* First ungrab the keys */
        ungrab_all_keys(conn);

        struct Mode *mode;
        Binding *bind;
        while (!SLIST_EMPTY(&modes)) {
            mode = SLIST_FIRST(&modes);
            FREE(mode->name);

            /* Clear the old binding list */
            bindings = mode->bindings;
            while (!TAILQ_EMPTY(bindings)) {
                bind = TAILQ_FIRST(bindings);
                TAILQ_REMOVE(bindings, bind, bindings);
                FREE(bind->translated_to);
                FREE(bind->command);
                FREE(bind);
            }
            FREE(bindings);
            SLIST_REMOVE(&modes, mode, Mode, modes);
        }

        struct Assignment *assign;
        while (!TAILQ_EMPTY(&assignments)) {
            assign = TAILQ_FIRST(&assignments);
            if (assign->type == A_TO_WORKSPACE)
                FREE(assign->dest.workspace);
            else if (assign->type == A_TO_OUTPUT)
                FREE(assign->dest.output);
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
            FREE(barconfig->outputs);
            FREE(barconfig->tray_output);
            FREE(barconfig->socket_path);
            FREE(barconfig->status_command);
            FREE(barconfig->i3bar_command);
            FREE(barconfig->font);
            FREE(barconfig->colors.background);
            FREE(barconfig->colors.statusline);
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
            TAILQ_REMOVE(&barconfigs, barconfig, configs);
            FREE(barconfig);
        }

/* Clear workspace names */
#if 0
        Workspace *ws;
        TAILQ_FOREACH(ws, workspaces, workspaces)
            workspace_set_name(ws, NULL);
#endif

        /* Invalidate pixmap caches in case font or colors changed */
        Con *con;
        TAILQ_FOREACH (con, &all_cons, all_cons)
            FREE(con->deco_render_params);

        /* Get rid of the current font */
        free_font();
    }

    SLIST_INIT(&modes);

    struct Mode *default_mode = scalloc(sizeof(struct Mode));
    default_mode->name = sstrdup("default");
    default_mode->bindings = scalloc(sizeof(struct bindings_head));
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
        x.border = get_colorpixel(cborder);                    \
        x.background = get_colorpixel(cbackground);            \
        x.text = get_colorpixel(ctext);                        \
        x.indicator = get_colorpixel(cindicator);              \
    } while (0)

    config.client.background = get_colorpixel("#000000");
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

    config.default_border = BS_NORMAL;
    config.default_floating_border = BS_NORMAL;
    config.default_border_width = logical_px(2);
    config.default_floating_border_width = logical_px(2);
    /* Set default_orientation to NO_ORIENTATION for auto orientation. */
    config.default_orientation = NO_ORIENTATION;

    /* Set default urgency reset delay to 500ms */
    if (config.workspace_urgency_timer == 0)
        config.workspace_urgency_timer = 0.5;

    parse_configuration(override_configpath);

    if (reload) {
        translate_keysyms();
        grab_all_keys(conn, false);
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

#if 0
    /* Set an empty name for every workspace which got no name */
    Workspace *ws;
    TAILQ_FOREACH(ws, workspaces, workspaces) {
            if (ws->name != NULL) {
                    /* If the font was not specified when the workspace name
                     * was loaded, we need to predict the text width now */
                    if (ws->text_width == 0)
                            ws->text_width = predict_text_width(global_conn,
                                            config.font, ws->name, ws->name_len);
                    continue;
            }

            workspace_set_name(ws, NULL);
    }
#endif
}
