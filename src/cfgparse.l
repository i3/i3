/*
 * vim:ts=4:sw=4:expandtab
 *
 */
%option nounput
%option noinput
%option noyy_top_state
%option stack

%{
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <xcb/xcb.h>

#include "data.h"
#include "config.h"
#include "log.h"
#include "util.h"
#include "libi3.h"

#include "cfgparse.tab.h"

int yycolumn = 1;

#define YY_DECL int yylex (struct context *context)

#define YY_USER_ACTION { \
    context->first_column = yycolumn; \
    context->last_column = yycolumn+yyleng-1; \
    yycolumn += yyleng; \
}

/* macro to first eat whitespace, then expect a string */
#define WS_STRING do { \
    yy_push_state(WANT_STRING); \
    yy_push_state(EAT_WHITESPACE); \
} while (0)

#define BAR_DOUBLE_COLOR do { \
    yy_push_state(BAR_COLOR); \
    yy_push_state(BAR_COLOR); \
} while (0)

%}

EOL     (\r?\n)

%s WANT_STRING
%s WANT_QSTRING
%s BINDSYM_COND
%s ASSIGN_COND
%s ASSIGN_TARGET_COND
%s COLOR_COND
%s OUTPUT_COND
%s FOR_WINDOW_COND
%s EAT_WHITESPACE

%x BUFFER_LINE
%x BAR
%x BAR_MODE
%x BAR_POSITION
%x BAR_COLORS
%x BAR_COLOR

%%

    {
        /* This is called when a new line is lexed. We only want the
         * first line to match to go into state BUFFER_LINE */
        if (context->line_number == 0) {
            context->line_number = 1;
            BEGIN(INITIAL);
            yy_push_state(BUFFER_LINE);
        }
    }

<BUFFER_LINE>^[^\r\n]*/{EOL}? {
    /* save whole line */
    context->line_copy = sstrdup(yytext);

    yyless(0);
    yy_pop_state();
    yy_set_bol(true);
    yycolumn = 1;
}

 /* This part of the lexer handles the bar {} blocks */
<BAR,BAR_MODE,BAR_POSITION,BAR_COLORS,BAR_COLOR>[ \t]+ { /* ignore whitespace */ ; }
<BAR>"{"                        { return '{'; }
<BAR>"}"                        { yy_pop_state(); return '}'; }
<BAR>^[ \t]*#[^\n]*             { return TOKCOMMENT; }
<BAR>output                     { WS_STRING; return TOK_BAR_OUTPUT; }
<BAR>tray_output                { WS_STRING; return TOK_BAR_TRAY_OUTPUT; }
<BAR>socket_path                { WS_STRING; return TOK_BAR_SOCKET_PATH; }
<BAR>mode                       { yy_push_state(BAR_MODE); return TOK_BAR_MODE; }
<BAR_MODE>hide                  { yy_pop_state(); return TOK_BAR_HIDE; }
<BAR_MODE>dock                  { yy_pop_state(); return TOK_BAR_DOCK; }
<BAR>position                   { yy_push_state(BAR_POSITION); return TOK_BAR_POSITION; }
<BAR_POSITION>bottom            { yy_pop_state(); return TOK_BAR_BOTTOM; }
<BAR_POSITION>top               { yy_pop_state(); return TOK_BAR_TOP; }
<BAR>status_command             { WS_STRING; return TOK_BAR_STATUS_COMMAND; }
<BAR>font                       { WS_STRING; return TOK_BAR_FONT; }
<BAR>workspace_buttons          { return TOK_BAR_WORKSPACE_BUTTONS; }
<BAR>verbose                    { return TOK_BAR_VERBOSE; }
<BAR>colors                     { yy_push_state(BAR_COLORS); return TOK_BAR_COLORS; }
<BAR_COLORS>"{"                 { return '{'; }
<BAR_COLORS>"}"                 { yy_pop_state(); return '}'; }
<BAR_COLORS>background          { yy_push_state(BAR_COLOR); return TOK_BAR_COLOR_BACKGROUND; }
<BAR_COLORS>statusline          { yy_push_state(BAR_COLOR); return TOK_BAR_COLOR_STATUSLINE; }
<BAR_COLORS>focused_workspace   { BAR_DOUBLE_COLOR; return TOK_BAR_COLOR_FOCUSED_WORKSPACE; }
<BAR_COLORS>active_workspace    { BAR_DOUBLE_COLOR; return TOK_BAR_COLOR_ACTIVE_WORKSPACE; }
<BAR_COLORS>inactive_workspace  { BAR_DOUBLE_COLOR; return TOK_BAR_COLOR_INACTIVE_WORKSPACE; }
<BAR_COLORS>urgent_workspace    { BAR_DOUBLE_COLOR; return TOK_BAR_COLOR_URGENT_WORKSPACE; }
<BAR_COLOR>#[0-9a-fA-F]+        { yy_pop_state(); yylval.string = sstrdup(yytext+1); return HEXCOLOR; }
<BAR,BAR_COLORS,BAR_MODE,BAR_POSITION>[a-zA-Z]+ { yylval.string = sstrdup(yytext); return WORD; }



<FOR_WINDOW_COND>"]"            { yy_pop_state(); return ']'; }
<ASSIGN_COND>"["                {
                                  /* this is the case for the new assign syntax
                                   * that uses criteria */
                                  yy_pop_state();
                                  yy_push_state(FOR_WINDOW_COND);
                                  /* afterwards we will be in ASSIGN_TARGET_COND */
                                  return '[';
                                }
<EAT_WHITESPACE>[ \t]*          { yy_pop_state(); }
<WANT_QSTRING>\"[^\"]+\"        {
                                  yy_pop_state();
                                  /* strip quotes */
                                  char *copy = sstrdup(yytext+1);
                                  copy[strlen(copy)-1] = '\0';
                                  yylval.string = copy;
                                  return STR;
                                }
<WANT_STRING>[^\n]+             { yy_pop_state(); yylval.string = sstrdup(yytext); return STR; }
<OUTPUT_COND>[a-zA-Z0-9_-]+     { yy_pop_state(); yylval.string = sstrdup(yytext); return OUTPUT; }
^[ \t]*#[^\n]*                  { return TOKCOMMENT; }
<COLOR_COND>#[0-9a-fA-F]+       { yy_pop_state(); yylval.string = sstrdup(yytext+1); return HEXCOLOR; }
<ASSIGN_TARGET_COND>[ \t]*→[ \t]*     { BEGIN(WANT_STRING); }
<ASSIGN_TARGET_COND>[ \t]+      { BEGIN(WANT_STRING); }
[0-9]+                          { yylval.number = atoi(yytext); return NUMBER; }
bar                             { yy_push_state(BAR); return TOK_BAR; }
mode                            { return TOKMODE; }
bind                            { yy_push_state(WANT_STRING); yy_push_state(EAT_WHITESPACE); yy_push_state(EAT_WHITESPACE); return TOKBINDCODE; }
bindcode                        { yy_push_state(WANT_STRING); yy_push_state(EAT_WHITESPACE); yy_push_state(EAT_WHITESPACE); return TOKBINDCODE; }
bindsym                         { yy_push_state(BINDSYM_COND); yy_push_state(EAT_WHITESPACE); return TOKBINDSYM; }
floating_modifier               { return TOKFLOATING_MODIFIER; }
workspace                       { return TOKWORKSPACE; }
output                          { yy_push_state(OUTPUT_COND); yy_push_state(EAT_WHITESPACE); return TOKOUTPUT; }
terminal                        { WS_STRING; return TOKTERMINAL; }
font                            { WS_STRING; return TOKFONT; }
assign                          { yy_push_state(ASSIGN_TARGET_COND); yy_push_state(ASSIGN_COND); return TOKASSIGN; }
set[^\n]*                       { return TOKCOMMENT; }
ipc-socket                      { WS_STRING; return TOKIPCSOCKET; }
ipc_socket                      { WS_STRING; return TOKIPCSOCKET; }
restart_state                   { WS_STRING; return TOKRESTARTSTATE; }
default_orientation             { return TOK_ORIENTATION; }
horizontal                      { return TOK_HORIZ; }
vertical                        { return TOK_VERT; }
auto                            { return TOK_AUTO; }
workspace_layout                { return TOK_WORKSPACE_LAYOUT; }
new_window                      { return TOKNEWWINDOW; }
new_float                       { return TOKNEWFLOAT; }
normal                          { return TOK_NORMAL; }
none                            { return TOK_NONE; }
1pixel                          { return TOK_1PIXEL; }
focus_follows_mouse             { return TOKFOCUSFOLLOWSMOUSE; }
force_focus_wrapping            { return TOK_FORCE_FOCUS_WRAPPING; }
force_xinerama                  { return TOK_FORCE_XINERAMA; }
workspace_auto_back_and_forth   { return TOK_WORKSPACE_AUTO_BAF; }
workspace_bar                   { return TOKWORKSPACEBAR; }
popup_during_fullscreen         { return TOK_POPUP_DURING_FULLSCREEN; }
ignore                          { return TOK_IGNORE; }
leave_fullscreen                { return TOK_LEAVE_FULLSCREEN; }
for_window                      {
                                  /* Example: for_window [class="urxvt"] border none
                                   *
                                   * First, we wait for the ']' that finishes a match (FOR_WINDOW_COND)
                                   * Then, we require a whitespace (EAT_WHITESPACE)
                                   * And the rest of the line is parsed as a string
                                   */
                                  yy_push_state(WANT_STRING);
                                  yy_push_state(EAT_WHITESPACE);
                                  yy_push_state(FOR_WINDOW_COND);
                                  return TOK_FOR_WINDOW;
                                }
default                         { /* yylval.number = MODE_DEFAULT; */return TOK_DEFAULT; }
stacking                        { /* yylval.number = MODE_STACK; */return TOK_STACKING; }
stacked                         { return TOK_STACKING; }
tabbed                          { /* yylval.number = MODE_TABBED; */return TOK_TABBED; }
stack-limit                     { return TOKSTACKLIMIT; }
cols                            { /* yylval.number = STACK_LIMIT_COLS; */return TOKSTACKLIMIT; }
rows                            { /* yylval.number = STACK_LIMIT_ROWS; */return TOKSTACKLIMIT; }
exec                            { WS_STRING; return TOKEXEC; }
exec_always                     { WS_STRING; return TOKEXEC_ALWAYS; }
client.background               { yy_push_state(COLOR_COND); yylval.single_color = &config.client.background; return TOKSINGLECOLOR; }
client.focused                  { yy_push_state(COLOR_COND); yy_push_state(COLOR_COND); yy_push_state(COLOR_COND); yylval.color = &config.client.focused; return TOKCOLOR; }
client.focused_inactive         { yy_push_state(COLOR_COND); yy_push_state(COLOR_COND); yy_push_state(COLOR_COND); yylval.color = &config.client.focused_inactive; return TOKCOLOR; }
client.unfocused                { yy_push_state(COLOR_COND); yy_push_state(COLOR_COND); yy_push_state(COLOR_COND); yylval.color = &config.client.unfocused; return TOKCOLOR; }
client.urgent                   { yy_push_state(COLOR_COND); yy_push_state(COLOR_COND); yy_push_state(COLOR_COND); yylval.color = &config.client.urgent; return TOKCOLOR; }
bar.focused                     { yy_push_state(COLOR_COND); yylval.color = &config.bar.focused; return TOKCOLOR; }
bar.unfocused                   { yy_push_state(COLOR_COND); yylval.color = &config.bar.unfocused; return TOKCOLOR; }
bar.urgent                      { yy_push_state(COLOR_COND); yylval.color = &config.bar.urgent; return TOKCOLOR; }
Mod1                            { yylval.number = BIND_MOD1; return MODIFIER; }
Mod2                            { yylval.number = BIND_MOD2; return MODIFIER; }
Mod3                            { yylval.number = BIND_MOD3; return MODIFIER; }
Mod4                            { yylval.number = BIND_MOD4; return MODIFIER; }
Mod5                            { yylval.number = BIND_MOD5; return MODIFIER; }
Mode_switch                     { yylval.number = BIND_MODE_SWITCH; return MODIFIER; }
control                         { return TOKCONTROL; }
ctrl                            { return TOKCONTROL; }
shift                           { return TOKSHIFT; }

class                           { yy_push_state(WANT_QSTRING); return TOK_CLASS; }
instance                        { yy_push_state(WANT_QSTRING); return TOK_INSTANCE; }
window_role                     { yy_push_state(WANT_QSTRING); return TOK_WINDOW_ROLE; }
id                              { yy_push_state(WANT_QSTRING); return TOK_ID; }
con_id                          { yy_push_state(WANT_QSTRING); return TOK_CON_ID; }
con_mark                        { yy_push_state(WANT_QSTRING); return TOK_MARK; }
title                           { yy_push_state(WANT_QSTRING); return TOK_TITLE; }

<*>{EOL}                        {
                                  FREE(context->line_copy);
                                  context->line_number++;
                                  yy_push_state(BUFFER_LINE);
                                }
<BINDSYM_COND>[ \t]+            { yy_pop_state(); yy_push_state(WANT_STRING); }
<OUTPUT_COND>[ \t]+             { yy_pop_state(); yy_push_state(WANT_STRING); }
[ \t]+                          { /* ignore whitespace */ ; }
\"[^\"]+\"                      {
                                  /* if ASSIGN_COND then */
                                  if (yy_start_stack_ptr > 0)
                                      yy_pop_state();
                                  /* yylval will be the string, but without quotes */
                                  char *copy = sstrdup(yytext+1);
                                  copy[strlen(copy)-1] = '\0';
                                  yylval.string = copy;
                                  return QUOTEDSTRING;
                                }
<ASSIGN_COND>[^ \t\"\[]+        { BEGIN(ASSIGN_TARGET_COND); yylval.string = sstrdup(yytext); return STR_NG; }
<BINDSYM_COND>[a-zA-Z0-9_]+     { yylval.string = sstrdup(yytext); return WORD; }
[a-zA-Z]+                       { yylval.string = sstrdup(yytext); return WORD; }
.                               { return (int)yytext[0]; }

<<EOF>> {
    while (yy_start_stack_ptr > 0)
        yy_pop_state();
    yyterminate();
}

%%
