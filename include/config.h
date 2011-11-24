/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
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
#include "libi3.h"

typedef struct Config Config;
typedef struct Barconfig Barconfig;
extern char *current_configpath;
extern Config config;
extern SLIST_HEAD(modes_head, Mode) modes;
extern TAILQ_HEAD(barconfig_head, Barconfig) barconfigs;

/**
 * Used during the config file lexing/parsing to keep the state of the lexer
 * in order to provide useful error messages in yyerror().
 *
 */
struct context {
    bool has_errors;
    bool has_warnings;

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

    char *ipc_socket_path;
    const char *restart_state_path;

    int default_layout;
    int container_stack_limit;
    int container_stack_limit_value;

    /** Default orientation for new containers */
    int default_orientation;

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

    /** Think of the following layout: Horizontal workspace with a tabbed
     * con on the left of the screen and a terminal on the right of the
     * screen. You are in the second container in the tabbed container and
     * focus to the right. By default, i3 will set focus to the terminal on
     * the right. If you are in the first container in the tabbed container
     * however, focusing to the left will wrap. This option forces i3 to
     * always wrap, which will result in you having to use "focus parent"
     * more often. */
    bool force_focus_wrapping;

    /** By default, use the RandR API for multi-monitor setups.
     * Unfortunately, the nVidia binary graphics driver doesn't support
     * this API. Instead, it only support the less powerful Xinerama API,
     * which can be enabled by this option.
     *
     * Note: this option takes only effect on the initial startup (eg.
     * reconfiguration is not possible). On startup, the list of screens
     * is fetched once and never updated. */
    bool force_xinerama;

    /** Automatic workspace back and forth switching. If this is set, a
     * switch to the currently active workspace will switch to the
     * previously focused one instead, making it possible to fast toggle
     * between two workspaces. */
    bool workspace_auto_back_and_forth;

    /** The default border style for new windows. */
    border_style_t default_border;

    /** The default border style for new floating windows. */
    border_style_t default_floating_border;

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
 * Holds the status bar configuration (i3bar). One of these structures is
 * created for each 'bar' block in the config.
 *
 */
struct Barconfig {
    /** Automatically generated ID for this bar config. Used by the bar process
     * to request a specific configuration. */
    char *id;

    /** Number of outputs in the outputs array */
    int num_outputs;
    /** Outputs on which this bar should show up on. We use an array for
     * simplicity (since we store just strings). */
    char **outputs;

    /** Output on which the tray should be shown. The special value of 'no'
     * disables the tray (it’s enabled by default). */
    char *tray_output;

    /** Path to the i3 IPC socket. This option is discouraged since programs
     * can find out the path by looking for the I3_SOCKET_PATH property on the
     * root window! */
    char *socket_path;

    /** Bar display mode (hide unless modifier is pressed or show in dock mode) */
    enum { M_DOCK = 0, M_HIDE = 1 } mode;

    /** Bar position (bottom by default). */
    enum { P_BOTTOM = 0, P_TOP = 1 } position;

    /** Command that should be run to execute i3bar, give a full path if i3bar is not
     * in your $PATH.
     * By default just 'i3bar' is executed. */
    char *i3bar_command;

    /** Command that should be run to get a statusline, for example 'i3status'.
     * Will be passed to the shell. */
    char *status_command;

    /** Font specification for all text rendered on the bar. */
    char *font;

    /** Hide workspace buttons? Configuration option is 'workspace_buttons no'
     * but we invert the bool to get the correct default when initializing with
     * zero. */
    bool hide_workspace_buttons;

    /** Enable verbose mode? Useful for debugging purposes. */
    bool verbose;

    struct bar_colors {
        char *background;
        char *statusline;

        char *focused_workspace_text;
        char *focused_workspace_bg;

        char *active_workspace_text;
        char *active_workspace_bg;

        char *inactive_workspace_text;
        char *inactive_workspace_bg;

        char *urgent_workspace_text;
        char *urgent_workspace_bg;
    } colors;

    TAILQ_ENTRY(Barconfig) configs;
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
void switch_mode(const char *new_mode);

/**
 * Returns a pointer to the Binding with the specified modifiers and keycode
 * or NULL if no such binding exists.
 *
 */
Binding *get_binding(uint16_t modifiers, xcb_keycode_t keycode);

/**
 * Kills the configerror i3-nagbar process, if any.
 *
 * Called when reloading/restarting.
 *
 * If wait_for_it is set (restarting), this function will waitpid(), otherwise,
 * ev is assumed to handle it (reloading).
 *
 */
void kill_configerror_nagbar(bool wait_for_it);

/* prototype for src/cfgparse.y */
void parse_file(const char *f);

#endif
