/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * src/config.c: Contains all functions handling the configuration file (calling
 * the parser (src/cfgparse.y) with the correct path, switching key bindings
 * mode).
 *
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <glob.h>

/* We need Xlib for XStringToKeysym */
#include <X11/Xlib.h>

#include <xcb/xcb_keysyms.h>

#include "i3.h"
#include "util.h"
#include "config.h"
#include "xcb.h"
#include "table.h"
#include "workspace.h"

Config config;
struct modes_head modes;

/*
 * This function resolves ~ in pathnames.
 *
 */
static char *glob_path(const char *path) {
        static glob_t globbuf;
        if (glob(path, GLOB_NOCHECK | GLOB_TILDE, NULL, &globbuf) < 0)
                die("glob() failed");
        char *result = sstrdup(globbuf.gl_pathc > 0 ? globbuf.gl_pathv[0] : path);
        globfree(&globbuf);
        return result;
}

/**
 * Ungrabs all keys, to be called before re-grabbing the keys because of a
 * mapping_notify event or a configuration file reload
 *
 */
void ungrab_all_keys(xcb_connection_t *conn) {
        LOG("Ungrabbing all keys\n");
        xcb_ungrab_key(conn, XCB_GRAB_ANY, root, XCB_BUTTON_MASK_ANY);
}

static void grab_keycode_for_binding(xcb_connection_t *conn, Binding *bind, uint32_t keycode) {
        LOG("Grabbing %d\n", keycode);
        if ((bind->mods & BIND_MODE_SWITCH) != 0)
                xcb_grab_key(conn, 0, root, 0, keycode,
                        XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_SYNC);
        else {
                /* Grab the key in all combinations */
                #define GRAB_KEY(modifier) xcb_grab_key(conn, 0, root, modifier, keycode, \
                                                        XCB_GRAB_MODE_SYNC, XCB_GRAB_MODE_ASYNC)
                GRAB_KEY(bind->mods);
                GRAB_KEY(bind->mods | xcb_numlock_mask);
                GRAB_KEY(bind->mods | xcb_numlock_mask | XCB_MOD_MASK_LOCK);
        }
}

/*
 * Grab the bound keys (tell X to send us keypress events for those keycodes)
 *
 */
void grab_all_keys(xcb_connection_t *conn) {
        Binding *bind;
        TAILQ_FOREACH(bind, bindings, bindings) {
                /* The easy case: the user specified a keycode directly. */
                if (bind->keycode > 0) {
                        grab_keycode_for_binding(conn, bind, bind->keycode);
                        continue;
                }

                /* We need to translate the symbol to a keycode */
                xcb_keysym_t keysym = XStringToKeysym(bind->symbol);
                if (keysym == NoSymbol) {
                        LOG("Could not translate string to key symbol: \"%s\"\n", bind->symbol);
                        continue;
                }

#ifdef OLD_XCB_KEYSYMS_API
                bind->number_keycodes = 1;
                xcb_keycode_t code = xcb_key_symbols_get_keycode(keysyms, keysym);
                LOG("Translated symbol \"%s\" to 1 keycode (%d)\n", bind->symbol, code);
                grab_keycode_for_binding(conn, bind, code);
                bind->translated_to = smalloc(sizeof(xcb_keycode_t));
                memcpy(bind->translated_to, &code, sizeof(xcb_keycode_t));
#else
                uint32_t last_keycode = 0;
                xcb_keycode_t *keycodes = xcb_key_symbols_get_keycode(keysyms, keysym);
                if (keycodes == NULL) {
                        LOG("Could not translate symbol \"%s\"\n", bind->symbol);
                        continue;
                }

                bind->number_keycodes = 0;

                for (xcb_keycode_t *walk = keycodes; *walk != 0; walk++) {
                        /* We hope duplicate keycodes will be returned in order
                         * and skip them */
                        if (last_keycode == *walk)
                                continue;
                        grab_keycode_for_binding(conn, bind, *walk);
                        last_keycode = *walk;
                        bind->number_keycodes++;
                }
                LOG("Translated symbol \"%s\" to %d keycode\n", bind->symbol, bind->number_keycodes);
                bind->translated_to = smalloc(bind->number_keycodes * sizeof(xcb_keycode_t));
                memcpy(bind->translated_to, keycodes, bind->number_keycodes * sizeof(xcb_keycode_t));
                free(keycodes);
#endif
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
                grab_all_keys(conn);
                return;
        }

        LOG("ERROR: Mode not found\n");
}

/*
 * Finds the configuration file to use (either the one specified by
 * override_configpath), the user’s one or the system default) and calls
 * parse_file().
 *
 */
static void parse_configuration(const char *override_configpath) {
        if (override_configpath != NULL) {
                parse_file(override_configpath);
                return;
        }

        FILE *handle;
        char *globbed = glob_path("~/.i3/config");
        if ((handle = fopen(globbed, "r")) == NULL) {
                if ((handle = fopen("/etc/i3/config", "r")) == NULL)
                        die("Neither \"%s\" nor /etc/i3/config could be opened\n", globbed);

                parse_file("/etc/i3/config");
                return;
        }

        parse_file(globbed);
        fclose(handle);
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
                        FREE(assign->windowclass_title);
                        TAILQ_REMOVE(&assignments, assign, assignments);
                        FREE(assign);
                }
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
                x.border = get_colorpixel(conn, cborder); \
                x.background = get_colorpixel(conn, cbackground); \
                x.text = get_colorpixel(conn, ctext); \
        } while (0)

        INIT_COLOR(config.client.focused, "#4c7899", "#285577", "#ffffff");
        INIT_COLOR(config.client.focused_inactive, "#333333", "#5f676a", "#ffffff");
        INIT_COLOR(config.client.unfocused, "#333333", "#222222", "#888888");
        INIT_COLOR(config.client.urgent, "#2f343a", "#900000", "#ffffff");
        INIT_COLOR(config.bar.focused, "#4c7899", "#285577", "#ffffff");
        INIT_COLOR(config.bar.unfocused, "#333333", "#222222", "#888888");
        INIT_COLOR(config.bar.urgent, "#2f343a", "#900000", "#ffffff");

        parse_configuration(override_configpath);

        if (reload)
                grab_all_keys(conn);

        REQUIRED_OPTION(font);

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
}
