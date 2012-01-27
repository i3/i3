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

char *cmd_criteria_init(Match *current_match);
char *cmd_criteria_match_windows(Match *current_match);
char *cmd_criteria_add(Match *current_match, char *ctype, char *cvalue);

char *cmd_move_con_to_workspace(Match *current_match, char *which);
char *cmd_move_con_to_workspace_name(Match *current_match, char *name);
char *cmd_resize(Match *current_match, char *way, char *direction, char *resize_px, char *resize_ppt);
char *cmd_border(Match *current_match, char *border_style_str);
char *cmd_nop(Match *current_match, char *comment);
char *cmd_append_layout(Match *current_match, char *path);
char *cmd_workspace(Match *current_match, char *which);
char *cmd_workspace_back_and_forth(Match *current_match);
char *cmd_workspace_name(Match *current_match, char *name);
char *cmd_mark(Match *current_match, char *mark);
char *cmd_mode(Match *current_match, char *mode);
char *cmd_move_con_to_output(Match *current_match, char *name);
char *cmd_floating(Match *current_match, char *floating_mode);
char *cmd_move_workspace_to_output(Match *current_match, char *name);
char *cmd_split(Match *current_match, char *direction);
char *cmd_kill(Match *current_match, char *kill_mode);
char *cmd_exec(Match *current_match, char *nosn, char *command);
char *cmd_focus_direction(Match *current_match, char *direction);
char *cmd_focus_window_mode(Match *current_match, char *window_mode);
char *cmd_focus_level(Match *current_match, char *level);
char *cmd_focus(Match *current_match);
char *cmd_fullscreen(Match *current_match, char *fullscreen_mode);
char *cmd_move_direction(Match *current_match, char *direction, char *px);
char *cmd_layout(Match *current_match, char *layout);
char *cmd_exit(Match *current_match);
char *cmd_reload(Match *current_match);
char *cmd_restart(Match *current_match);
char *cmd_open(Match *current_match);
char *cmd_focus_output(Match *current_match, char *name);
char *cmd_move_scratchpad(Match *current_match);
char *cmd_scratchpad_show(Match *current_match);

#endif
