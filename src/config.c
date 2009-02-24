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

#include "i3.h"
#include "util.h"
#include "config.h"

Config config;

/*
 * Reads the configuration from the given file
 *
 */
void load_configuration(const char *configfile) {
#define OPTION_STRING(name) \
        if (strcasecmp(key, #name) == 0) { \
                config.name = sstrdup(value); \
                continue; \
        }

#define REQUIRED_OPTION(name) \
        if (config.name == NULL) \
                die("You did not specify required configuration option " #name "\n");

        /* Clear the old config or initialize the data structure */
        memset(&config, 0, sizeof(config));

        /* check if the file exists */
        struct stat buf;
        if (stat(configfile, &buf) < 0)
                return;

        FILE *handle = fopen(configfile, "r");
        if (handle == NULL)
                die("Could not open configfile\n");
        char key[512], value[512], buffer[1026];

        while (!feof(handle)) {
                if (fgets(buffer, 1024, handle) == NULL) {
                        /* fgets returns NULL on EOF and on error, so see which one it is. */
                        if (feof(handle))
                                break;
                        die("Could not read configuration file\n");
                }

                /* sscanf implicitly strips whitespace. Also, we skip comments and empty lines. */
                if (sscanf(buffer, "%s %[^\n]", key, value) < 1 ||
                    key[0] == '#' || strlen(key) < 3)
                        continue;

                OPTION_STRING(terminal);
                OPTION_STRING(font);

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
                        printf("keycode = %d, modifiers = %d, command = *%s*\n", keycode, modifiers, rest);
                        Binding *new = smalloc(sizeof(Binding));
                        new->keycode = keycode;
                        new->mods = modifiers;
                        new->command = sstrdup(rest);
                        TAILQ_INSERT_TAIL(&bindings, new, bindings);
                        continue;
                }

                fprintf(stderr, "Unknown configfile option: %s\n", key);
                exit(1);
        }
        fclose(handle);

        REQUIRED_OPTION(terminal);
        REQUIRED_OPTION(font);

        return;
}
