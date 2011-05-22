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

%}

EOL     (\r?\n)

%s WANT_STRING
%s WANT_QSTRING
%s BINDSYM_COND
%s ASSIGN_COND
%s COLOR_COND
%s OUTPUT_COND
%s FOR_WINDOW_COND
%s EAT_WHITESPACE
%x BUFFER_LINE

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


<FOR_WINDOW_COND>"]"            { yy_pop_state(); return ']'; }
<EAT_WHITESPACE>[ \t]*          { yy_pop_state(); }
<WANT_QSTRING>\"[^\"]+\"        {
                                  yy_pop_state();
                                  /* strip quotes */
                                  char *copy = sstrdup(yytext+1);
                                  copy[strlen(copy)-1] = '\0';
                                  yylval.string = copy;
                                  return STR;
                                }
<WANT_STRING>[^\n]+             { BEGIN(INITIAL); yylval.string = sstrdup(yytext); return STR; }
<OUTPUT_COND>[a-zA-Z0-9_-]+     { yylval.string = sstrdup(yytext); return OUTPUT; }
^[ \t]*#[^\n]*                  { return TOKCOMMENT; }
<COLOR_COND>[0-9a-fA-F]+        { yylval.string = sstrdup(yytext); return HEX; }
[0-9]+                          { yylval.number = atoi(yytext); return NUMBER; }
mode                            { return TOKMODE; }
bind                            { yy_push_state(WANT_STRING); yy_push_state(EAT_WHITESPACE); yy_push_state(EAT_WHITESPACE); return TOKBINDCODE; }
bindcode                        { yy_push_state(WANT_STRING); yy_push_state(EAT_WHITESPACE); yy_push_state(EAT_WHITESPACE); return TOKBINDCODE; }
bindsym                         { yy_push_state(BINDSYM_COND); yy_push_state(EAT_WHITESPACE); return TOKBINDSYM; }
floating_modifier               { BEGIN(INITIAL); return TOKFLOATING_MODIFIER; }
workspace                       { BEGIN(INITIAL); return TOKWORKSPACE; }
output                          { yy_push_state(OUTPUT_COND); yy_push_state(EAT_WHITESPACE); return TOKOUTPUT; }
screen                          {
                                  /* for compatibility until v3.φ */
                                  ELOG("Assignments to screens are DEPRECATED and will not work. " \
                                       "Please replace them with assignments to outputs.\n");
                                  yy_push_state(OUTPUT_COND); yy_push_state(EAT_WHITESPACE);
                                  return TOKOUTPUT;
                                }
terminal                        { WS_STRING; return TOKTERMINAL; }
font                            { WS_STRING; return TOKFONT; }
assign                          { BEGIN(ASSIGN_COND); return TOKASSIGN; }
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
normal                          { return TOK_NORMAL; }
none                            { return TOK_NONE; }
1pixel                          { return TOK_1PIXEL; }
focus_follows_mouse             { return TOKFOCUSFOLLOWSMOUSE; }
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
client.background               { BEGIN(COLOR_COND); yylval.single_color = &config.client.background; return TOKSINGLECOLOR; }
client.focused                  { BEGIN(COLOR_COND); yylval.color = &config.client.focused; return TOKCOLOR; }
client.focused_inactive         { BEGIN(COLOR_COND); yylval.color = &config.client.focused_inactive; return TOKCOLOR; }
client.unfocused                { BEGIN(COLOR_COND); yylval.color = &config.client.unfocused; return TOKCOLOR; }
client.urgent                   { BEGIN(COLOR_COND); yylval.color = &config.client.urgent; return TOKCOLOR; }
bar.focused                     { BEGIN(COLOR_COND); yylval.color = &config.bar.focused; return TOKCOLOR; }
bar.unfocused                   { BEGIN(COLOR_COND); yylval.color = &config.bar.unfocused; return TOKCOLOR; }
bar.urgent                      { BEGIN(COLOR_COND); yylval.color = &config.bar.urgent; return TOKCOLOR; }
Mod1                            { yylval.number = BIND_MOD1; return MODIFIER; }
Mod2                            { yylval.number = BIND_MOD2; return MODIFIER; }
Mod3                            { yylval.number = BIND_MOD3; return MODIFIER; }
Mod4                            { yylval.number = BIND_MOD4; return MODIFIER; }
Mod5                            { yylval.number = BIND_MOD5; return MODIFIER; }
Mode_switch                     { yylval.number = BIND_MODE_SWITCH; return MODIFIER; }
control                         { return TOKCONTROL; }
ctrl                            { return TOKCONTROL; }
shift                           { return TOKSHIFT; }
→                               { return TOKARROW; }

class                           { yy_push_state(WANT_QSTRING); return TOK_CLASS; }
id                              { yy_push_state(WANT_QSTRING); return TOK_ID; }
con_id                          { yy_push_state(WANT_QSTRING); return TOK_CON_ID; }
con_mark                        { yy_push_state(WANT_QSTRING); return TOK_MARK; }
title                           { yy_push_state(WANT_QSTRING); return TOK_TITLE; }

{EOL}                           {
                                  FREE(context->line_copy);
                                  context->line_number++;
                                  BEGIN(INITIAL);
                                  yy_push_state(BUFFER_LINE);
                                }
<BINDSYM_COND>[ \t]+            { BEGIN(WANT_STRING); }
<OUTPUT_COND>[ \t]+             { BEGIN(WANT_STRING); }
[ \t]+                          { /* ignore whitespace */ ; }
\"[^\"]+\"                      {
                                  /* if ASSIGN_COND then */
                                  BEGIN(INITIAL);
                                  /* yylval will be the string, but without quotes */
                                  char *copy = sstrdup(yytext+1);
                                  copy[strlen(copy)-1] = '\0';
                                  yylval.string = copy;
                                  return QUOTEDSTRING;
                                }
<ASSIGN_COND>[^ \t]+            { BEGIN(INITIAL); yylval.string = sstrdup(yytext); return STR_NG; }
<BINDSYM_COND>[a-zA-Z0-9_]+     { yylval.string = sstrdup(yytext); return WORD; }
[a-zA-Z]+                       { yylval.string = sstrdup(yytext); return WORD; }
.                               { return (int)yytext[0]; }

<<EOF>> {
    while (yy_start_stack_ptr > 0)
        yy_pop_state();
    yyterminate();
}

%%
