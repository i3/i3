/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * commands.c: all command functions (see commands_parser.c)
 *
 */
#ifndef _COMMANDS_H
#define _COMMANDS_H

/*
 * Helper data structure for an operation window (window on which the operation
 * will be performed). Used to build the TAILQ owindows.
 *
 */
typedef struct owindow {
    Con *con;
    TAILQ_ENTRY(owindow) owindows;
} owindow;

typedef TAILQ_HEAD(owindows_head, owindow) owindows_head;

/**
 * Initializes the specified 'Match' data structure and the initial state of
 * commands.c for matching target windows of a command.
 *
 */
char *cmd_criteria_init(Match *current_match);

/**
 * A match specification just finished (the closing square bracket was found),
 * so we filter the list of owindows.
 *
 */
char *cmd_criteria_match_windows(Match *current_match);

/**
 * Interprets a ctype=cvalue pair and adds it to the current match
 * specification.
 *
 */
char *cmd_criteria_add(Match *current_match, char *ctype, char *cvalue);

/**
 * Implementation of 'move [window|container] [to] workspace
 * next|prev|next_on_output|prev_on_output'.
 *
 */
char *cmd_move_con_to_workspace(Match *current_match, char *which);

/**
 * Implementation of 'move [window|container] [to] workspace <name>'.
 *
 */
char *cmd_move_con_to_workspace_name(Match *current_match, char *name);

/**
 * Implementation of 'resize grow|shrink <direction> [<px> px] [or <ppt> ppt]'.
 *
 */
char *cmd_resize(Match *current_match, char *way, char *direction, char *resize_px, char *resize_ppt);

/**
 * Implementation of 'border normal|none|1pixel|toggle'.
 *
 */
char *cmd_border(Match *current_match, char *border_style_str);

/**
 * Implementation of 'nop <comment>'.
 *
 */
char *cmd_nop(Match *current_match, char *comment);

/**
 * Implementation of 'append_layout <path>'.
 *
 */
char *cmd_append_layout(Match *current_match, char *path);

/**
 * Implementation of 'workspace next|prev|next_on_output|prev_on_output'.
 *
 */
char *cmd_workspace(Match *current_match, char *which);

/**
 * Implementation of 'workspace back_and_forth'.
 *
 */
char *cmd_workspace_back_and_forth(Match *current_match);

/**
 * Implementation of 'workspace <name>'
 *
 */
char *cmd_workspace_name(Match *current_match, char *name);

/**
 * Implementation of 'mark <mark>'
 *
 */
char *cmd_mark(Match *current_match, char *mark);

/**
 * Implementation of 'mode <string>'.
 *
 */
char *cmd_mode(Match *current_match, char *mode);

/**
 * Implementation of 'move [window|container] [to] output <str>'.
 *
 */
char *cmd_move_con_to_output(Match *current_match, char *name);

/**
 * Implementation of 'floating enable|disable|toggle'
 *
 */
char *cmd_floating(Match *current_match, char *floating_mode);

/**
 * Implementation of 'move workspace to [output] <str>'.
 *
 */
char *cmd_move_workspace_to_output(Match *current_match, char *name);

/**
 * Implementation of 'split v|h|vertical|horizontal'.
 *
 */
char *cmd_split(Match *current_match, char *direction);

/**
 * Implementaiton of 'kill [window|client]'.
 *
 */
char *cmd_kill(Match *current_match, char *kill_mode);

/**
 * Implementation of 'exec [--no-startup-id] <command>'.
 *
 */
char *cmd_exec(Match *current_match, char *nosn, char *command);

/**
 * Implementation of 'focus left|right|up|down'.
 *
 */
char *cmd_focus_direction(Match *current_match, char *direction);

/**
 * Implementation of 'focus tiling|floating|mode_toggle'.
 *
 */
char *cmd_focus_window_mode(Match *current_match, char *window_mode);

/**
 * Implementation of 'focus parent|child'.
 *
 */
char *cmd_focus_level(Match *current_match, char *level);

/**
 * Implementation of 'focus'.
 *
 */
char *cmd_focus(Match *current_match);

/**
 * Implementation of 'fullscreen [global]'.
 *
 */
char *cmd_fullscreen(Match *current_match, char *fullscreen_mode);

/**
 * Implementation of 'move <direction> [<pixels> [px]]'.
 *
 */
char *cmd_move_direction(Match *current_match, char *direction, char *px);

/**
 * Implementation of 'layout default|stacked|stacking|tabbed'.
 *
 */
char *cmd_layout(Match *current_match, char *layout);

/**
 * Implementaiton of 'exit'.
 *
 */
char *cmd_exit(Match *current_match);

/**
 * Implementaiton of 'reload'.
 *
 */
char *cmd_reload(Match *current_match);

/**
 * Implementaiton of 'restart'.
 *
 */
char *cmd_restart(Match *current_match);

/**
 * Implementaiton of 'open'.
 *
 */
char *cmd_open(Match *current_match);

/**
 * Implementation of 'focus output <output>'.
 *
 */
char *cmd_focus_output(Match *current_match, char *name);

/**
 * Implementation of 'move scratchpad'.
 *
 */
char *cmd_move_scratchpad(Match *current_match);

/**
 * Implementation of 'scratchpad show'.
 *
 */
char *cmd_scratchpad_show(Match *current_match);

#endif
