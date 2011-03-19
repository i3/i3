/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * src/config.c: Contains all functions handling the configuration file (calling
 * the parser (src/cfgparse.y) with the correct path, switching key bindings
 * mode).
 *
 */

/* We need Xlib for XStringToKeysym */
#include <X11/Xlib.h>
#include <wordexp.h>

#include "all.h"

const char *current_configpath = NULL;
Config config;
struct modes_head modes;

/**
 * Ungrabs all keys, to be called before re-grabbing the keys because of a
 * mapping_notify event or a configuration file reload
 *
 */
void ungrab_all_keys(xcb_connection_t *conn) {
        DLOG("Ungrabbing all keys\n");
        xcb_ungrab_key(conn, XCB_GRAB_ANY, root, XCB_BUTTON_MASK_ANY);
}

static void grab_keycode_for_binding(xcb_connection_t *conn, Binding *bind, uint32_t keycode) {
        DLOG("Grabbing %d\n", keycode);
        /* Grab the key in all combinations */
        #define GRAB_KEY(modifier) \
                do { \
                        xcb_grab_key(conn, 0, root, modifier, keycode, \
                                     XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC); \
                } while (0)
        int mods = bind->mods;
        if ((bind->mods & BIND_MODE_SWITCH) != 0) {
                mods &= ~BIND_MODE_SWITCH;
                if (mods == 0)
                        mods = XCB_MOD_MASK_ANY;
        }
        GRAB_KEY(mods);
        GRAB_KEY(mods | xcb_numlock_mask);
        GRAB_KEY(mods | xcb_numlock_mask | XCB_MOD_MASK_LOCK);
}

/*
 * Returns a pointer to the Binding with the specified modifiers and keycode
 * or NULL if no such binding exists.
 *
 */
Binding *get_binding(uint16_t modifiers, xcb_keycode_t keycode) {
        Binding *bind;

        TAILQ_FOREACH(bind, bindings, bindings) {
                /* First compare the modifiers */
                if (bind->mods != modifiers)
                        continue;

                /* If a symbol was specified by the user, we need to look in
                 * the array of translated keycodes for the event’s keycode */
                if (bind->symbol != NULL) {
                        if (memmem(bind->translated_to,
                                   bind->number_keycodes * sizeof(xcb_keycode_t),
                                   &keycode, sizeof(xcb_keycode_t)) != NULL)
                                break;
                } else {
                        /* This case is easier: The user specified a keycode */
                        if (bind->keycode == keycode)
                                break;
                }
        }

        return (bind == TAILQ_END(bindings) ? NULL : bind);
}

/*
 * Translates keysymbols to keycodes for all bindings which use keysyms.
 *
 */
void translate_keysyms() {
        Binding *bind;
        TAILQ_FOREACH(bind, bindings, bindings) {
                if (bind->keycode > 0)
                        continue;

                /* We need to translate the symbol to a keycode */
                xcb_keysym_t keysym = XStringToKeysym(bind->symbol);
                if (keysym == NoSymbol) {
                        ELOG("Could not translate string to key symbol: \"%s\"\n", bind->symbol);
                        continue;
                }

                uint32_t last_keycode = 0;
                xcb_keycode_t *keycodes = xcb_key_symbols_get_keycode(keysyms, keysym);
                if (keycodes == NULL) {
                        DLOG("Could not translate symbol \"%s\"\n", bind->symbol);
                        continue;
                }

                bind->number_keycodes = 0;

                for (xcb_keycode_t *walk = keycodes; *walk != 0; walk++) {
                        /* We hope duplicate keycodes will be returned in order
                         * and skip them */
                        if (last_keycode == *walk)
                                continue;
                        last_keycode = *walk;
                        bind->number_keycodes++;
                }
                DLOG("Translated symbol \"%s\" to %d keycode\n", bind->symbol, bind->number_keycodes);
                bind->translated_to = smalloc(bind->number_keycodes * sizeof(xcb_keycode_t));
                memcpy(bind->translated_to, keycodes, bind->number_keycodes * sizeof(xcb_keycode_t));
                free(keycodes);
        }
}

/*
 * Grab the bound keys (tell X to send us keypress events for those keycodes)
 *
 */
void grab_all_keys(xcb_connection_t *conn, bool bind_mode_switch) {
        Binding *bind;
        TAILQ_FOREACH(bind, bindings, bindings) {
                if ((bind_mode_switch && (bind->mods & BIND_MODE_SWITCH) == 0) ||
                    (!bind_mode_switch && (bind->mods & BIND_MODE_SWITCH) != 0))
                        continue;

                /* The easy case: the user specified a keycode directly. */
                if (bind->keycode > 0) {
                        grab_keycode_for_binding(conn, bind, bind->keycode);
                        continue;
                }

                xcb_keycode_t *walk = bind->translated_to;
                for (int i = 0; i < bind->number_keycodes; i++)
                        grab_keycode_for_binding(conn, bind, *walk);
        }
}

/*
 * Switches the key bindings to the given mode, if the mode exists
 *
 */
void switch_mode(xcb_connection_t *conn, const char *new_mode) {
        struct Mode *mode;

        LOG("Switching to mode %s\n", new_mode);

        SLIST_FOREACH(mode, &modes, modes) {
                if (strcasecmp(mode->name, new_mode) != 0)
                        continue;

                ungrab_all_keys(conn);
                bindings = mode->bindings;
                translate_keysyms();
                grab_all_keys(conn, false);
                return;
        }

        ELOG("ERROR: Mode not found\n");
}

/*
 * Get the path of the first configuration file found. Checks the home directory
 * first, then the system directory first, always taking into account the XDG
 * Base Directory Specification ($XDG_CONFIG_HOME, $XDG_CONFIG_DIRS)
 *
 */
static char *get_config_path() {
        char *xdg_config_home, *xdg_config_dirs, *config_path;

        /* 1: check the traditional path under the home directory */
        config_path = resolve_tilde("~/.i3/config");
        if (path_exists(config_path))
                return config_path;

        /* 2: check for $XDG_CONFIG_HOME/i3/config */
        if ((xdg_config_home = getenv("XDG_CONFIG_HOME")) == NULL)
                xdg_config_home = "~/.config";

        xdg_config_home = resolve_tilde(xdg_config_home);
        if (asprintf(&config_path, "%s/i3/config", xdg_config_home) == -1)
                die("asprintf() failed");
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
                if (asprintf(&config_path, "%s/i3/config", tok) == -1)
                        die("asprintf() failed");
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
                "~/.i3/config, $XDG_CONFIG_HOME/i3/config, "
                SYSCONFDIR "i3/config and $XDG_CONFIG_DIRS/i3/config)");
}

/*
 * Finds the configuration file to use (either the one specified by
 * override_configpath), the user’s one or the system default) and calls
 * parse_file().
 *
 */
static void parse_configuration(const char *override_configpath) {
    static const char *saved_configpath = NULL;

    if (override_configpath != NULL) {
        saved_configpath = override_configpath;
        current_configpath = override_configpath;
        parse_file(override_configpath);
        return;
    }
    else if (saved_configpath != NULL) {
        current_configpath = saved_configpath;
        parse_file(saved_configpath);
        return;
    }

    char *path = get_config_path();
    DLOG("Parsing configfile %s\n", path);
    current_configpath = path;
    parse_file(path);
    free(path);
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

#if 0
                struct Assignment *assign;
                while (!TAILQ_EMPTY(&assignments)) {
                        assign = TAILQ_FIRST(&assignments);
                        FREE(assign->windowclass_title);
                        TAILQ_REMOVE(&assignments, assign, assignments);
                        FREE(assign);
                }
#endif

                /* Clear workspace names */
#if 0
                Workspace *ws;
                TAILQ_FOREACH(ws, workspaces, workspaces)
                        workspace_set_name(ws, NULL);
#endif
        }

        SLIST_INIT(&modes);

        struct Mode *default_mode = scalloc(sizeof(struct Mode));
        default_mode->name = sstrdup("default");
        default_mode->bindings = scalloc(sizeof(struct bindings_head));
        TAILQ_INIT(default_mode->bindings);
        SLIST_INSERT_HEAD(&modes, default_mode, modes);

        bindings = default_mode->bindings;

#define REQUIRED_OPTION(name) \
        if (config.name == NULL) \
                die("You did not specify required configuration option " #name "\n");

        /* Clear the old config or initialize the data structure */
        memset(&config, 0, sizeof(config));

        /* Initialize default colors */
#define INIT_COLOR(x, cborder, cbackground, ctext) \
        do { \
                x.border = get_colorpixel(cborder); \
                x.background = get_colorpixel(cbackground); \
                x.text = get_colorpixel(ctext); \
        } while (0)

        config.client.background = get_colorpixel("#000000");
        INIT_COLOR(config.client.focused, "#4c7899", "#285577", "#ffffff");
        INIT_COLOR(config.client.focused_inactive, "#333333", "#5f676a", "#ffffff");
        INIT_COLOR(config.client.unfocused, "#333333", "#222222", "#888888");
        INIT_COLOR(config.client.urgent, "#2f343a", "#900000", "#ffffff");
        INIT_COLOR(config.bar.focused, "#4c7899", "#285577", "#ffffff");
        INIT_COLOR(config.bar.unfocused, "#333333", "#222222", "#888888");
        INIT_COLOR(config.bar.urgent, "#2f343a", "#900000", "#ffffff");

        config.default_border = BS_NORMAL;
        /* Set default_orientation to NO_ORIENTATION for auto orientation. */
        config.default_orientation = NO_ORIENTATION;

        parse_configuration(override_configpath);

        if (reload) {
                translate_keysyms();
                grab_all_keys(conn, false);
        }

        if (config.font.id == 0) {
                ELOG("You did not specify required configuration option \"font\"\n");
                config.font = load_font("fixed", true);
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
