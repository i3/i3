/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * include/config.h: Contains all structs/variables for
 * the configurable part of i3
 *
 */

#ifndef _CONFIG_H
#define _CONFIG_H

#include "queue.h"

typedef struct Config Config;
extern Config config;

/**
 * Part of the struct Config. It makes sense to group colors for background,
 * border and text as every element in i3 has them (window decorations, bar).
 *
 */
struct Colortriple {
        uint32_t border;
        uint32_t background;
        uint32_t text;
};

/**
 * Holds a user-assigned variable for parsing the configuration file. The key
 * is replaced by value in every following line of the file.
 *
 */
struct Variable {
        char *key;
        char *value;

        SLIST_ENTRY(Variable) variables;
};

/**
 * Holds part of the configuration (the part which is not already in dedicated
 * structures in include/data.h).
 *
 */
struct Config {
        const char *terminal;
        const char *font;

        /** The modifier which needs to be pressed in combination with your mouse
         * buttons to do things with floating windows (move, resize) */
        uint32_t floating_modifier;

        /* Color codes are stored here */
        struct config_client {
                struct Colortriple focused;
                struct Colortriple focused_inactive;
                struct Colortriple unfocused;
        } client;
        struct config_bar {
                struct Colortriple focused;
                struct Colortriple unfocused;
        } bar;
};

/**
 * Reads the configuration from ~/.i3/config or /etc/i3/config if not found.
 *
 * If you specify override_configpath, only this path is used to look for a
 * configuration file.
 *
 */
void load_configuration(xcb_connection_t *conn, const char *override_configfile);

#endif
