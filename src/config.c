/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
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

/* prototype for src/cfgparse.y, will be cleaned up as soon as we completely
 * switched to the new scanner/parser. */
void parse_file(const char *f);

Config config;

bool config_use_lexer = false;

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

/*
 * This function does a very simple replacement of each instance of key with value.
 *
 */
static void replace_variable(char *buffer, const char *key, const char *value) {
        char *pos;
        /* To prevent endless recursions when the user makes an error configuring,
         * we stop after 100 replacements. That should be vastly more than enough. */
        int c = 0;
        while ((pos = strcasestr(buffer, key)) != NULL && c++ < 100) {
                char *rest = pos + strlen(key);
                *pos = '\0';
                char *replaced;
                asprintf(&replaced, "%s%s%s", buffer, value, rest);
                /* Hm, this is a bit ugly, but sizeof(buffer) = 4, as it’s just a pointer.
                 * So we need to hard-code the dimensions here. */
                strncpy(buffer, replaced, 1026);
                free(replaced);
        }
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
        TAILQ_FOREACH(bind, &bindings, bindings) {
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

                xcb_keycode_t *keycodes = xcb_key_symbols_get_keycode(keysyms, keysym);
                if (keycodes == NULL) {
                        LOG("Could not translate symbol \"%s\"\n", bind->symbol);
                        continue;
                }

                uint32_t last_keycode = 0;
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
        }
}

/*
 * Reads the configuration from ~/.i3/config or /etc/i3/config if not found.
 *
 * If you specify override_configpath, only this path is used to look for a
 * configuration file.
 *
 */
void load_configuration(xcb_connection_t *conn, const char *override_configpath, bool reload) {
        if (reload) {
                /* First ungrab the keys */
                ungrab_all_keys(conn);

                /* Clear the old binding and assignment lists */
                Binding *bind;
                while (!TAILQ_EMPTY(&bindings)) {
                        bind = TAILQ_FIRST(&bindings);
                        TAILQ_REMOVE(&bindings, bind, bindings);
                        FREE(bind->command);
                        FREE(bind);
                }

                struct Assignment *assign;
                while (!TAILQ_EMPTY(&assignments)) {
                        assign = TAILQ_FIRST(&assignments);
                        FREE(assign->windowclass_title);
                        TAILQ_REMOVE(&assignments, assign, assignments);
                        FREE(assign);
                }
        }

        SLIST_HEAD(variables_head, Variable) variables;

#define OPTION_STRING(name) \
        if (strcasecmp(key, #name) == 0) { \
                config.name = sstrdup(value); \
                continue; \
        }

#define REQUIRED_OPTION(name) \
        if (config.name == NULL) \
                die("You did not specify required configuration option " #name "\n");

#define OPTION_COLORTRIPLE(opt, name) \
        if (strcasecmp(key, opt) == 0) { \
                char border[8], background[8], text[8]; \
                memset(border, 0, sizeof(border)); \
                memset(background, 0, sizeof(background)); \
                memset(text, 0, sizeof(text)); \
                border[0] = background[0] = text[0] = '#'; \
                if (sscanf(value, "#%06[0-9a-fA-F] #%06[0-9a-fA-F] #%06[0-9a-fA-F]", \
                    border + 1, background + 1, text + 1) != 3 || \
                    strlen(border) != 7 || \
                    strlen(background) != 7 || \
                    strlen(text) != 7) \
                        die("invalid color code line: %s\n", value); \
                config.name.border = get_colorpixel(conn, border); \
                config.name.background = get_colorpixel(conn, background); \
                config.name.text = get_colorpixel(conn, text); \
                continue; \
        }

        /* Clear the old config or initialize the data structure */
        memset(&config, 0, sizeof(config));

        SLIST_INIT(&variables);

        /* Initialize default colors */
        config.client.focused.border = get_colorpixel(conn, "#4c7899");
        config.client.focused.background = get_colorpixel(conn, "#285577");
        config.client.focused.text = get_colorpixel(conn, "#ffffff");

        config.client.focused_inactive.border = get_colorpixel(conn, "#4c7899");
        config.client.focused_inactive.background = get_colorpixel(conn, "#555555");
        config.client.focused_inactive.text = get_colorpixel(conn, "#ffffff");

        config.client.unfocused.border = get_colorpixel(conn, "#333333");
        config.client.unfocused.background = get_colorpixel(conn, "#222222");
        config.client.unfocused.text = get_colorpixel(conn, "#888888");

        config.client.urgent.border = get_colorpixel(conn, "#2f343a");
        config.client.urgent.background = get_colorpixel(conn, "#900000");
        config.client.urgent.text = get_colorpixel(conn, "#ffffff");

        config.bar.focused.border = get_colorpixel(conn, "#4c7899");
        config.bar.focused.background = get_colorpixel(conn, "#285577");
        config.bar.focused.text = get_colorpixel(conn, "#ffffff");

        config.bar.unfocused.border = get_colorpixel(conn, "#333333");
        config.bar.unfocused.background = get_colorpixel(conn, "#222222");
        config.bar.unfocused.text = get_colorpixel(conn, "#888888");

        config.bar.urgent.border = get_colorpixel(conn, "#2f343a");
        config.bar.urgent.background = get_colorpixel(conn, "#900000");
        config.bar.urgent.text = get_colorpixel(conn, "#ffffff");

        if (config_use_lexer) {
                /* Yes, this will be cleaned up soon. */
                if (override_configpath != NULL) {
                        parse_file(override_configpath);
                } else {
                        FILE *handle;
                        char *globbed = glob_path("~/.i3/config");
                        if ((handle = fopen(globbed, "r")) == NULL) {
                                if ((handle = fopen("/etc/i3/config", "r")) == NULL) {
                                        die("Neither \"%s\" nor /etc/i3/config could be opened\n", globbed);
                                } else {
                                        parse_file("/etc/i3/config");
                                }
                        } else {
                                parse_file(globbed);
                        }
                }
                if (reload)
                        grab_all_keys(conn);
        } else {

        FILE *handle;
        if (override_configpath != NULL) {
                if ((handle = fopen(override_configpath, "r")) == NULL)
                        die("Could not open configfile \"%s\".\n", override_configpath);
        } else {
                /* We first check for ~/.i3/config, then for /etc/i3/config */
                char *globbed = glob_path("~/.i3/config");
                if ((handle = fopen(globbed, "r")) == NULL)
                        if ((handle = fopen("/etc/i3/config", "r")) == NULL)
                                die("Neither \"%s\" nor /etc/i3/config could be opened\n", globbed);
                free(globbed);
        }
        char key[512], value[512], buffer[1026];

        while (!feof(handle)) {
                if (fgets(buffer, 1024, handle) == NULL) {
                        /* fgets returns NULL on EOF and on error, so see which one it is. */
                        if (feof(handle))
                                break;
                        die("Could not read configuration file\n");
                }

                if (config.terminal != NULL)
                        replace_variable(buffer, "$terminal", config.terminal);

                /* Replace all custom variables */
                struct Variable *current;
                SLIST_FOREACH(current, &variables, variables)
                        replace_variable(buffer, current->key, current->value);

                /* sscanf implicitly strips whitespace. Also, we skip comments and empty lines. */
                if (sscanf(buffer, "%s %[^\n]", key, value) < 1 ||
                    key[0] == '#' || strlen(key) < 3)
                        continue;

                OPTION_STRING(terminal);
                OPTION_STRING(font);

                /* Colors */
                OPTION_COLORTRIPLE("client.focused", client.focused);
                OPTION_COLORTRIPLE("client.focused_inactive", client.focused_inactive);
                OPTION_COLORTRIPLE("client.unfocused", client.unfocused);
                OPTION_COLORTRIPLE("client.urgent", client.urgent);
                OPTION_COLORTRIPLE("bar.focused", bar.focused);
                OPTION_COLORTRIPLE("bar.unfocused", bar.unfocused);
                OPTION_COLORTRIPLE("bar.urgent", bar.urgent);

                /* exec-lines (autostart) */
                if (strcasecmp(key, "exec") == 0) {
                        struct Autostart *new = smalloc(sizeof(struct Autostart));
                        new->command = sstrdup(value);
                        TAILQ_INSERT_TAIL(&autostarts, new, autostarts);
                        continue;
                }

                /* key bindings */
                if (strcasecmp(key, "bind") == 0 || strcasecmp(key, "bindsym") == 0) {
                        #define CHECK_MODIFIER(name) \
                                if (strncasecmp(walk, #name, strlen(#name)) == 0) { \
                                        modifiers |= BIND_##name; \
                                        walk += strlen(#name) + 1; \
                                        continue; \
                                }
                        char *walk = value, *rest;
                        uint32_t modifiers = 0;

                        while (*walk != '\0') {
                                /* Need to check for Mod1-5, Ctrl, Shift, Mode_switch */
                                CHECK_MODIFIER(SHIFT);
                                CHECK_MODIFIER(CONTROL);
                                CHECK_MODIFIER(MODE_SWITCH);
                                CHECK_MODIFIER(MOD1);
                                CHECK_MODIFIER(MOD2);
                                CHECK_MODIFIER(MOD3);
                                CHECK_MODIFIER(MOD4);
                                CHECK_MODIFIER(MOD5);

                                /* No modifier found? Then we’re done with this step */
                                break;
                        }

                        Binding *new = scalloc(sizeof(Binding));

                        /* Now check for the keycode or copy the symbol */
                        if (strcasecmp(key, "bind") == 0) {
                                int keycode = strtol(walk, &rest, 10);
                                if (!rest || *rest != ' ')
                                        die("Invalid binding (keycode)\n");
                                new->keycode = keycode;
                        } else {
                                rest = walk;
                                char *sym = rest;
                                while (*rest != '\0' && *rest != ' ')
                                        rest++;
                                if (*rest != ' ')
                                        die("Invalid binding (keysym)\n");
#if defined(__OpenBSD__)
                                size_t len = strlen(sym);
                                if (len > (rest - sym))
                                        len = (rest - sym);
                                new->symbol = smalloc(len + 1);
                                memcpy(new->symbol, sym, len+1);
                                new->symbol[len]='\0';
#else
                                new->symbol = strndup(sym, (rest - sym));
#endif
                        }
                        rest++;
                        LOG("keycode = %d, symbol = %s, modifiers = %d, command = *%s*\n", new->keycode, new->symbol, modifiers, rest);
                        new->mods = modifiers;
                        new->command = sstrdup(rest);
                        TAILQ_INSERT_TAIL(&bindings, new, bindings);
                        continue;
                }

                if (strcasecmp(key, "floating_modifier") == 0) {
                        char *walk = value;
                        uint32_t modifiers = 0;

                        while (*walk != '\0') {
                                /* Need to check for Mod1-5, Ctrl, Shift, Mode_switch */
                                CHECK_MODIFIER(SHIFT);
                                CHECK_MODIFIER(CONTROL);
                                CHECK_MODIFIER(MODE_SWITCH);
                                CHECK_MODIFIER(MOD1);
                                CHECK_MODIFIER(MOD2);
                                CHECK_MODIFIER(MOD3);
                                CHECK_MODIFIER(MOD4);
                                CHECK_MODIFIER(MOD5);

                                /* No modifier found? Then we’re done with this step */
                                break;
                        }

                        LOG("Floating modifiers = %d\n", modifiers);
                        config.floating_modifier = modifiers;
                        continue;
                }

                /* workspace "workspace number" [screen <screen>] ["name of the workspace"]
                 * with screen := <number> | <position>, e.g. screen 1280 or screen 1 */
                if (strcasecmp(key, "name") == 0 || strcasecmp(key, "workspace") == 0) {
                        LOG("workspace: %s\n",value);
                        char *ws_str = sstrdup(value);
                        char *end = strchr(ws_str, ' ');
                        if (end == NULL)
                                die("Malformed name, couln't find terminating space\n");
                        *end = '\0';

                        /* Strip trailing whitespace */
                        while (strlen(value) > 0 && value[strlen(value)-1] == ' ')
                                value[strlen(value)-1] = '\0';

                        int ws_num = atoi(ws_str);

                        if (ws_num < 1 || ws_num > 10)
                                die("Malformed name, invalid workspace number\n");

                        /* find the name */
                        char *name = value;
                        name += strlen(ws_str) + 1;

                        if (strncasecmp(name, "screen ", strlen("screen ")) == 0) {
                                char *screen = strdup(name + strlen("screen "));
                                if ((end = strchr(screen, ' ')) != NULL)
                                        *end = '\0';
                                LOG("Setting preferred screen for workspace %d to \"%s\"\n", ws_num, screen);
                                workspace_get(ws_num-1)->preferred_screen = screen;

                                name += strlen("screen ") + strlen(screen);
                        }

                        /* Strip leading whitespace */
                        while (*name != '\0' && *name == ' ')
                                name++;

                        LOG("rest to parse = %s\n", name);

                        if (name == '\0') {
                                free(ws_str);
                                continue;
                        }

                        LOG("setting name to \"%s\"\n", name);

                        if (*name != '\0')
                                workspace_set_name(&(workspaces[ws_num - 1]), name);
                        free(ws_str);
                        continue;
                }

                /* assign window class[/window title] → workspace */
                if (strcasecmp(key, "assign") == 0) {
                        LOG("assign: \"%s\"\n", value);
                        char *class_title;
                        char *target;
                        char *end;

                        /* If the window class/title is quoted we skip quotes */
                        if (value[0] == '"') {
                                class_title = sstrdup(value+1);
                                end = strchr(class_title, '"');
                        } else {
                                class_title = sstrdup(value);
                                /* If it is not quoted, we terminate it at the first space */
                                end = strchr(class_title, ' ');
                        }
                        if (end == NULL)
                                die("Malformed assignment, couldn't find terminating quote\n");
                        *end = '\0';

                        /* Strip trailing whitespace */
                        while (strlen(value) > 0 && value[strlen(value)-1] == ' ')
                                value[strlen(value)-1] = '\0';

                        /* The target is the last argument separated by a space */
                        if ((target = strrchr(value, ' ')) == NULL)
                                die("Malformed assignment, couldn't find target (\"%s\")\n", value);
                        target++;

                        if (strchr(target, '~') == NULL && (atoi(target) < 1 || atoi(target) > 10))
                                die("Malformed assignment, invalid workspace number\n");

                        LOG("assignment parsed: \"%s\" to \"%s\"\n", class_title, target);

                        struct Assignment *new = scalloc(sizeof(struct Assignment));
                        new->windowclass_title = class_title;
                        if (strchr(target, '~') != NULL)
                                new->floating = ASSIGN_FLOATING_ONLY;

                        while (*target == '~')
                                target++;

                        if (atoi(target) >= 1) {
                                if (new->floating == ASSIGN_FLOATING_ONLY)
                                        new->floating = ASSIGN_FLOATING;
                                new->workspace = atoi(target);
                        }
                        TAILQ_INSERT_TAIL(&assignments, new, assignments);

                        LOG("Assignment loaded: \"%s\":\n", class_title);
                        if (new->floating != ASSIGN_FLOATING_ONLY)
                                LOG(" to workspace %d\n", new->workspace);

                        if (new->floating != ASSIGN_FLOATING_NO)
                                LOG(" will be floating\n");

                        continue;
                }

                /* set a custom variable */
                if (strcasecmp(key, "set") == 0) {
                        if (value[0] != '$')
                                die("Malformed variable assignment, name has to start with $\n");

                        /* get key/value for this variable */
                        char *v_key = value, *v_value;
                        if ((v_value = strstr(value, " ")) == NULL)
                                die("Malformed variable assignment, need a value\n");

                        *(v_value++) = '\0';

                        struct Variable *new = scalloc(sizeof(struct Variable));
                        new->key = sstrdup(v_key);
                        new->value = sstrdup(v_value);
                        SLIST_INSERT_HEAD(&variables, new, variables);
                        LOG("Got new variable %s = %s\n", v_key, v_value);
                        continue;
                }

                if (strcasecmp(key, "ipc-socket") == 0) {
                        config.ipc_socket_path = sstrdup(value);
                        continue;
                }

                die("Unknown configfile option: %s\n", key);
        }
        /* now grab all keys again */
        if (reload)
                grab_all_keys(conn);
        fclose(handle);

        while (!SLIST_EMPTY(&variables)) {
                struct Variable *v = SLIST_FIRST(&variables);
                SLIST_REMOVE_HEAD(&variables, variables);
                free(v->key);
                free(v->value);
                free(v);
        }
        }

        REQUIRED_OPTION(terminal);
        REQUIRED_OPTION(font);

        /* Set an empty name for every workspace which got no name */
        for (int i = 0; i < num_workspaces; i++) {
                Workspace *ws = &(workspaces[i]);
                if (ws->name != NULL) {
                        /* If the font was not specified when the workspace name
                         * was loaded, we need to predict the text width now */
                        if (ws->text_width == 0)
                                ws->text_width = predict_text_width(global_conn,
                                                config.font, ws->name, ws->name_len);
                        continue;
                }

                workspace_set_name(&(workspaces[i]), NULL);
        }
 
        return;
}
