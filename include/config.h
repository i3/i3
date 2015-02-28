/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * include/config.h: Contains all structs/variables for the configurable
 * part of i3 as well as functions handling the configuration file (calling
 * the parser (src/config_parse.c) with the correct path, switching key
 * bindings mode).
 *
 */
#pragma once

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
    uint32_t indicator;
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

    layout_t default_layout;
    int container_stack_limit;
    int container_stack_limit_value;
    int default_border_width;
    int default_floating_border_width;

    /** Default orientation for new containers */
    int default_orientation;

    /** By default, focus follows mouse. If the user explicitly wants to
     * turn this off (and instead rely only on the keyboard for changing
     * focus), we allow him to do this with this relatively special option.
     * It is not planned to add any different focus models. */
    bool disable_focus_follows_mouse;

    /** By default, when switching focus to a window on a different output
     * (e.g. focusing a window on workspace 3 on output VGA-1, coming from
     * workspace 2 on LVDS-1), the mouse cursor is warped to the center of
     * that window.
     *
     * With the mouse_warping option, you can control when the mouse cursor
     * should be warped. "none" disables warping entirely, whereas "output"
     * is the default behavior described above. */
    warping_t mouse_warping;

    /** Remove borders if they are adjacent to the screen edge.
     * This is useful if you are reaching scrollbar on the edge of the
     * screen or do not want to waste a single pixel of displayspace.
     * By default, this is disabled. */
    adjacent_t hide_edge_borders;

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

    /** Overwrites output detection (for testing), see src/fake_outputs.c */
    char *fake_outputs;

    /** Automatic workspace back and forth switching. If this is set, a
     * switch to the currently active workspace will switch to the
     * previously focused one instead, making it possible to fast toggle
     * between two workspaces. */
    bool workspace_auto_back_and_forth;

    /** By default, urgency is cleared immediately when switching to another
     * workspace leads to focusing the con with the urgency hint. When having
     * multiple windows on that workspace, the user needs to guess which
     * application raised the event. To prevent this, the reset of the urgency
     * flag can be delayed using an urgency timer. */
    float workspace_urgency_timer;

    /** The default border style for new windows. */
    border_style_t default_border;

    /** The default border style for new floating windows. */
    border_style_t default_floating_border;

    /** The modifier which needs to be pressed in combination with your mouse
     * buttons to do things with floating windows (move, resize) */
    uint32_t floating_modifier;

    /** Maximum and minimum dimensions of a floating window */
    int32_t floating_maximum_width;
    int32_t floating_maximum_height;
    int32_t floating_minimum_width;
    int32_t floating_minimum_height;

    /* Color codes are stored here */
    struct config_client {
        uint32_t background;
        struct Colortriple focused;
        struct Colortriple focused_inactive;
        struct Colortriple unfocused;
        struct Colortriple urgent;
        struct Colortriple placeholder;
    } client;
    struct config_bar {
        struct Colortriple focused;
        struct Colortriple unfocused;
        struct Colortriple urgent;
    } bar;

    /** What should happen when a new popup is opened during fullscreen mode */
    enum {
        /* display (and focus) the popup when it belongs to the fullscreen
         * window only. */
        PDF_SMART = 0,

        /* leave fullscreen mode unconditionally */
        PDF_LEAVE_FULLSCREEN = 1,

        /* just ignore the popup, that is, don’t map it */
        PDF_IGNORE = 2,
    } popup_during_fullscreen;

    /* The number of currently parsed barconfigs */
    int number_barconfigs;
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

    /** Bar display mode (hide unless modifier is pressed or show in dock mode or always hide in invisible mode) */
    enum { M_DOCK = 0,
           M_HIDE = 1,
           M_INVISIBLE = 2 } mode;

    /* The current hidden_state of the bar, which indicates whether it is hidden or shown */
    enum { S_HIDE = 0,
           S_SHOW = 1 } hidden_state;

    /** Bar modifier (to show bar when in hide mode). */
    enum {
        M_NONE = 0,
        M_CONTROL = 1,
        M_SHIFT = 2,
        M_MOD1 = 3,
        M_MOD2 = 4,
        M_MOD3 = 5,
        M_MOD4 = 6,
        M_MOD5 = 7
    } modifier;

    /** Command that should be run when mouse wheel up button is pressed over
     * i3bar to override the default behavior. */
    char *wheel_up_cmd;

    /** Command that should be run when mouse wheel down button is pressed over
     * i3bar to override the default behavior. */
    char *wheel_down_cmd;

    /** Bar position (bottom by default). */
    enum { P_BOTTOM = 0,
           P_TOP = 1 } position;

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

    /** Strip workspace numbers? Configuration option is
     * 'strip_workspace_numbers yes'. */
    bool strip_workspace_numbers;

    /** Hide mode button? Configuration option is 'binding_mode_indicator no'
     * but we invert the bool for the same reason as hide_workspace_buttons.*/
    bool hide_binding_mode_indicator;

    /** Enable verbose mode? Useful for debugging purposes. */
    bool verbose;

    struct bar_colors {
        char *background;
        char *statusline;
        char *separator;

        char *focused_workspace_border;
        char *focused_workspace_bg;
        char *focused_workspace_text;

        char *active_workspace_border;
        char *active_workspace_bg;
        char *active_workspace_text;

        char *inactive_workspace_border;
        char *inactive_workspace_bg;
        char *inactive_workspace_text;

        char *urgent_workspace_border;
        char *urgent_workspace_bg;
        char *urgent_workspace_text;
    } colors;

    TAILQ_ENTRY(Barconfig) configs;
};

/**
 * Finds the configuration file to use (either the one specified by
 * override_configpath), the user’s one or the system default) and calls
 * parse_file().
 *
 * If you specify override_configpath, only this path is used to look for a
 * configuration file.
 *
 * If use_nagbar is false, don't try to start i3-nagbar but log the errors to
 * stdout/stderr instead.
 *
 */
bool parse_configuration(const char *override_configpath, bool use_nagbar);

/**
 * Reads the configuration from ~/.i3/config or /etc/i3/config if not found.
 *
 * If you specify override_configpath, only this path is used to look for a
 * configuration file.
 *
 */
void load_configuration(xcb_connection_t *conn, const char *override_configfile, bool reload);

/**
 * Ungrabs all keys, to be called before re-grabbing the keys because of a
 * mapping_notify event or a configuration file reload
 *
 */
void ungrab_all_keys(xcb_connection_t *conn);

/**
 * Sends the current bar configuration as an event to all barconfig_update listeners.
 *
 */
void update_barconfig();

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
