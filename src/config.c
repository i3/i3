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

#include "i3.h"
#include "util.h"
#include "config.h"
#include "xcb.h"

Config config;

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
        LOG("Replacing %s with %s\n", key, value);
        while ((pos = strcasestr(buffer, key)) != NULL && c++ < 100) {
                LOG("replacing variable %s in \"%s\" with \"%s\"\n", key, buffer, value);
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

/*
 * Reads the configuration from ~/.i3/config or /etc/i3/config if not found.
 *
 * If you specify override_configpath, only this path is used to look for a
 * configuration file.
 *
 */
void load_configuration(xcb_connection_t *conn, const char *override_configpath) {
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

        config.bar.focused.border = get_colorpixel(conn, "#4c7899");
        config.bar.focused.background = get_colorpixel(conn, "#285577");
        config.bar.focused.text = get_colorpixel(conn, "#ffffff");

        config.bar.unfocused.border = get_colorpixel(conn, "#333333");
        config.bar.unfocused.background = get_colorpixel(conn, "#222222");
        config.bar.unfocused.text = get_colorpixel(conn, "#888888");

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
                OPTION_COLORTRIPLE("bar.focused", bar.focused);
                OPTION_COLORTRIPLE("bar.unfocused", bar.unfocused);

                /* exec-lines (autostart) */
                if (strcasecmp(key, "exec") == 0) {
                        struct Autostart *new = smalloc(sizeof(struct Autostart));
                        new->command = sstrdup(value);
                        TAILQ_INSERT_TAIL(&autostarts, new, autostarts);
                        continue;
                }

                /* key bindings */
                if (strcasecmp(key, "bind") == 0) {
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

                        /* Now check for the keycode */
                        int keycode = strtol(walk, &rest, 10);
                        if (!rest || *rest != ' ')
                                die("Invalid binding\n");
                        rest++;
                        LOG("keycode = %d, modifiers = %d, command = *%s*\n", keycode, modifiers, rest);
                        Binding *new = smalloc(sizeof(Binding));
                        new->keycode = keycode;
                        new->mods = modifiers;
                        new->command = sstrdup(rest);
                        TAILQ_INSERT_TAIL(&bindings, new, bindings);
                        continue;
                }

                /* assign window class[/window title] → workspace */
                if (strcasecmp(key, "assign") == 0) {
                        LOG("assign: \"%s\"\n", value);
                        char *class_title = sstrdup(value);
                        char *target;

                        /* If the window class/title is quoted we skip quotes */
                        if (class_title[0] == '"') {
                                class_title++;
                                char *end = strchr(class_title, '"');
                                if (end == NULL)
                                        die("Malformed assignment, couldn't find terminating quote\n");
                                *end = '\0';
                        } else {
                                /* If it is not quoted, we terminate it at the first space */
                                char *end = strchr(class_title, ' ');
                                if (end == NULL)
                                        die("Malformed assignment, couldn't find terminating space\n");
                                *end = '\0';
                        }

                        /* The target is the last argument separated by a space */
                        if ((target = strrchr(value, ' ')) == NULL)
                                die("Malformed assignment, couldn't find target\n");
                        target++;

                        if (atoi(target) < 1 || atoi(target) > 10)
                                die("Malformed assignment, invalid workspace number\n");

                        LOG("assignment parsed: \"%s\" to \"%s\"\n", class_title, target);

                        struct Assignment *new = smalloc(sizeof(struct Assignment));
                        new->windowclass_title = class_title;
                        new->workspace = atoi(target);
                        TAILQ_INSERT_TAIL(&assignments, new, assignments);
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

                die("Unknown configfile option: %s\n", key);
        }
        fclose(handle);

        REQUIRED_OPTION(terminal);
        REQUIRED_OPTION(font);
 
        return;
}
