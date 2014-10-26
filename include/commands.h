/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * commands.c: all command functions (see commands_parser.c)
 *
 */
#pragma once

#include "commands_parser.h"

/** The beginning of the prototype for every cmd_ function. */
#define I3_CMD Match *current_match, struct CommandResultIR *cmd_output

/**
 * Initializes the specified 'Match' data structure and the initial state of
 * commands.c for matching target windows of a command.
 *
 */
void cmd_criteria_init(I3_CMD);

/**
 * A match specification just finished (the closing square bracket was found),
 * so we filter the list of owindows.
 *
 */
void cmd_criteria_match_windows(I3_CMD);

/**
 * Interprets a ctype=cvalue pair and adds it to the current match
 * specification.
 *
 */
void cmd_criteria_add(I3_CMD, char *ctype, char *cvalue);

/**
 * Implementation of 'move [window|container] [to] workspace
 * next|prev|next_on_output|prev_on_output'.
 *
 */
void cmd_move_con_to_workspace(I3_CMD, char *which);

/**
 * Implementation of 'move [window|container] [to] workspace back_and_forth'.
 *
 */
void cmd_move_con_to_workspace_back_and_forth(I3_CMD);

/**
 * Implementation of 'move [window|container] [to] workspace <name>'.
 *
 */
void cmd_move_con_to_workspace_name(I3_CMD, char *name);

/**
 * Implementation of 'move [window|container] [to] workspace number <number>'.
 *
 */
void cmd_move_con_to_workspace_number(I3_CMD, char *which);

/**
 * Implementation of 'resize grow|shrink <direction> [<px> px] [or <ppt> ppt]'.
 *
 */
void cmd_resize(I3_CMD, char *way, char *direction, char *resize_px, char *resize_ppt);

/**
 * Implementation of 'border normal|none|1pixel|toggle'.
 *
 */
void cmd_border(I3_CMD, char *border_style_str, char *border_width);

/**
 * Implementation of 'nop <comment>'.
 *
 */
void cmd_nop(I3_CMD, char *comment);

/**
 * Implementation of 'append_layout <path>'.
 *
 */
void cmd_append_layout(I3_CMD, char *path);

/**
 * Implementation of 'workspace next|prev|next_on_output|prev_on_output'.
 *
 */
void cmd_workspace(I3_CMD, char *which);

/**
 * Implementation of 'workspace number <number>'
 *
 */
void cmd_workspace_number(I3_CMD, char *which);

/**
 * Implementation of 'workspace back_and_forth'.
 *
 */
void cmd_workspace_back_and_forth(I3_CMD);

/**
 * Implementation of 'workspace <name>'
 *
 */
void cmd_workspace_name(I3_CMD, char *name);

/**
 * Implementation of 'mark <mark>'
 *
 */
void cmd_mark(I3_CMD, char *mark);

/**
 * Implementation of 'unmark [mark]'
 *
 */
void cmd_unmark(I3_CMD, char *mark);

/**
 * Implementation of 'mode <string>'.
 *
 */
void cmd_mode(I3_CMD, char *mode);

/**
 * Implementation of 'move [window|container] [to] output <str>'.
 *
 */
void cmd_move_con_to_output(I3_CMD, char *name);

/**
 * Implementation of 'floating enable|disable|toggle'
 *
 */
void cmd_floating(I3_CMD, char *floating_mode);

/**
 * Implementation of 'move workspace to [output] <str>'.
 *
 */
void cmd_move_workspace_to_output(I3_CMD, char *name);

/**
 * Implementation of 'split v|h|vertical|horizontal'.
 *
 */
void cmd_split(I3_CMD, char *direction);

/**
 * Implementation of 'kill [window|client]'.
 *
 */
void cmd_kill(I3_CMD, char *kill_mode_str);

/**
 * Implementation of 'exec [--no-startup-id] <command>'.
 *
 */
void cmd_exec(I3_CMD, char *nosn, char *command);

/**
 * Implementation of 'focus left|right|up|down'.
 *
 */
void cmd_focus_direction(I3_CMD, char *direction);

/**
 * Implementation of 'focus tiling|floating|mode_toggle'.
 *
 */
void cmd_focus_window_mode(I3_CMD, char *window_mode);

/**
 * Implementation of 'focus parent|child'.
 *
 */
void cmd_focus_level(I3_CMD, char *level);

/**
 * Implementation of 'focus'.
 *
 */
void cmd_focus(I3_CMD);

/**
 * Implementation of 'fullscreen [enable|disable|toggle] [global]'.
 *
 */
void cmd_fullscreen(I3_CMD, char *action, char *fullscreen_mode);

/**
 * Implementation of 'move <direction> [<pixels> [px]]'.
 *
 */
void cmd_move_direction(I3_CMD, char *direction, char *move_px);

/**
 * Implementation of 'layout default|stacked|stacking|tabbed|splitv|splith'.
 *
 */
void cmd_layout(I3_CMD, char *layout_str);

/**
 * Implementation of 'layout toggle [all|split]'.
 *
 */
void cmd_layout_toggle(I3_CMD, char *toggle_mode);

/**
 * Implementation of 'exit'.
 *
 */
void cmd_exit(I3_CMD);

/**
 * Implementation of 'reload'.
 *
 */
void cmd_reload(I3_CMD);

/**
 * Implementation of 'restart'.
 *
 */
void cmd_restart(I3_CMD);

/**
 * Implementation of 'open'.
 *
 */
void cmd_open(I3_CMD);

/**
 * Implementation of 'focus output <output>'.
 *
 */
void cmd_focus_output(I3_CMD, char *name);

/**
 * Implementation of 'move [window|container] [to] [absolute] position <px> [px] <px> [px]
 *
 */
void cmd_move_window_to_position(I3_CMD, char *method, char *x, char *y);

/**
 * Implementation of 'move [window|container] [to] [absolute] position center
 *
 */
void cmd_move_window_to_center(I3_CMD, char *method);

/**
 * Implementation of 'move scratchpad'.
 *
 */
void cmd_move_scratchpad(I3_CMD);

/**
 * Implementation of 'scratchpad show'.
 *
 */
void cmd_scratchpad_show(I3_CMD);

/**
 * Implementation of 'rename workspace <name> to <name>'
 *
 */
void cmd_rename_workspace(I3_CMD, char *old_name, char *new_name);

/**
 * Implementation of 'bar (hidden_state hide|show|toggle)|(mode dock|hide|invisible|toggle) [<bar_id>]'
 *
 */
void cmd_bar(I3_CMD, char *bar_type, char *bar_value, char *bar_id);

/*
 * Implementation of 'shmlog <size>|toggle|on|off'
 *
 */
void cmd_shmlog(I3_CMD, char *argument);

/*
 * Implementation of 'debuglog toggle|on|off'
 *
 */
void cmd_debuglog(I3_CMD, char *argument);
