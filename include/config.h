/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009-2010 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * include/config.h: Contains all structs/variables for the configurable
 * part of i3 as well as functions handling the configuration file (calling
 * the parser (src/cfgparse.y) with the correct path, switching key bindings
 * mode).
 *
 */

#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdbool.h>
#include "queue.h"
#include "i3.h"

typedef struct Config Config;
extern Config config;
extern SLIST_HEAD(modes_head, Mode) modes;

/**
 * Used during the config file lexing/parsing to keep the state of the lexer
 * in order to provide useful error messages in yyerror().
 *
 */
struct context {
        int line_number;
        char *line_copy;
        const char *filename;

        char *compact_error;

        /* These are the same as in YYLTYPE */
        int first_column;
        int last_column;
};

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
        char *next_match;

        SLIST_ENTRY(Variable) variables;
};

/**
 * The configuration file can contain multiple sets of bindings. Apart from the
 * default set (name == "default"), you can specify other sets and change the
 * currently active set of bindings by using the "mode <name>" command.
 *
 */
struct Mode {
        char *name;
        struct bindings_head *bindings;

        SLIST_ENTRY(Mode) modes;
};

/**
 * Holds part of the configuration (the part which is not already in dedicated
 * structures in include/data.h).
 *
 */
struct Config {
        const char *terminal;
        i3Font font;

        const char *ipc_socket_path;
        const char *restart_state_path;

        int container_mode;
        int container_stack_limit;
        int container_stack_limit_value;

        /** By default, focus follows mouse. If the user explicitly wants to
         * turn this off (and instead rely only on the keyboard for changing
         * focus), we allow him to do this with this relatively special option.
         * It is not planned to add any different focus models. */
        bool disable_focus_follows_mouse;

        /** By default, a workspace bar is drawn at the bottom of the screen.
         * If you want to have a more fancy bar, it is recommended to replace
         * the whole bar by dzen2, for example using the i3-wsbar script which
         * comes with i3. Thus, you can turn it off entirely. */
        bool disable_workspace_bar;

        /** The default border style for new windows. */
        border_style_t default_border;

        /** The modifier which needs to be pressed in combination with your mouse
         * buttons to do things with floating windows (move, resize) */
        uint32_t floating_modifier;

        /* Color codes are stored here */
        struct config_client {
                uint32_t background;
                struct Colortriple focused;
                struct Colortriple focused_inactive;
                struct Colortriple unfocused;
                struct Colortriple urgent;
        } client;
        struct config_bar {
                struct Colortriple focused;
                struct Colortriple unfocused;
                struct Colortriple urgent;
        } bar;

        /** What should happen when a new popup is opened during fullscreen mode */
        enum {
                PDF_LEAVE_FULLSCREEN = 0,
                PDF_IGNORE = 1
        } popup_during_fullscreen;
};

/**
 * Reads the configuration from ~/.i3/config or /etc/i3/config if not found.
 *
 * If you specify override_configpath, only this path is used to look for a
 * configuration file.
 *
 */
void load_configuration(xcb_connection_t *conn, const char *override_configfile, bool reload);

/**
 * Translates keysymbols to keycodes for all bindings which use keysyms.
 *
 */
void translate_keysyms();

/**
 * Ungrabs all keys, to be called before re-grabbing the keys because of a
 * mapping_notify event or a configuration file reload
 *
 */
void ungrab_all_keys(xcb_connection_t *conn);

/**
 * Grab the bound keys (tell X to send us keypress events for those keycodes)
 *
 */
void grab_all_keys(xcb_connection_t *conn, bool bind_mode_switch);

/**
 * Switches the key bindings to the given mode, if the mode exists
 *
 */
void switch_mode(xcb_connection_t *conn, const char *new_mode);

/**
 * Returns a pointer to the Binding with the specified modifiers and keycode
 * or NULL if no such binding exists.
 *
 */
Binding *get_binding(uint16_t modifiers, xcb_keycode_t keycode);

/* prototype for src/cfgparse.y */
void parse_file(const char *f);

#endif
