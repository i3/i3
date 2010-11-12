/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2010 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * cmdparse.l: the lexer for commands you send to i3 (or bind on keys)
 *
 */
%option nounput
%option noinput
%option noyy_top_state
%option stack

%{
#include <stdio.h>
#include <string.h>
#include "cmdparse.tab.h"

#include "config.h"
#include "util.h"

int cmdyycolumn = 1;

#define YY_DECL int yylex (struct context *context)

#define YY_USER_ACTION { \
    context->first_column = cmdyycolumn; \
    context->last_column = cmdyycolumn+yyleng-1; \
    cmdyycolumn += yyleng; \
}

%}

EOL (\r?\n)

/* handle everything up to \n as a string */
%s WANT_STRING
/* first expect a whitespace, then a string */
%s WANT_WS_STRING
/* handle a quoted string or everything up to the next whitespace */
%s WANT_QSTRING

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
    cmdyycolumn = 1;
}

<WANT_STRING>[^\n]+             { BEGIN(INITIAL); cmdyylval.string = sstrdup(yytext); return STR; }
<WANT_WS_STRING>[ \t]*          { BEGIN(WANT_STRING); return WHITESPACE; }
<WANT_QSTRING>\"[^\"]+\"        {
                                  BEGIN(INITIAL);
                                  /* strip quotes */
                                  char *copy = sstrdup(yytext+1);
                                  copy[strlen(copy)-1] = '\0';
                                  cmdyylval.string = copy;
                                  return STR;
                                }

[ \t]*                          { return WHITESPACE; }
attach                          { return TOK_ATTACH; }
exec                            { BEGIN(WANT_WS_STRING); return TOK_EXEC; }
exit                            { return TOK_EXIT; }
reload                          { return TOK_RELOAD; }
restart                         { return TOK_RESTART; }
kill                            { return TOK_KILL; }
fullscreen                      { return TOK_FULLSCREEN; }
global                          { return TOK_GLOBAL; }
layout                          { return TOK_LAYOUT; }
default                         { return TOK_DEFAULT; }
stacked                         { return TOK_STACKED; }
stacking                        { return TOK_STACKED; }
tabbed                          { return TOK_TABBED; }
border                          { return TOK_BORDER; }
normal                          { return TOK_NORMAL; }
none                            { return TOK_NONE; }
1pixel                          { return TOK_1PIXEL; }
mode                            { return TOK_MODE; }
tiling                          { return TOK_TILING; }
floating                        { return TOK_FLOATING; }
toggle                          { return TOK_TOGGLE; }
workspace                       { BEGIN(WANT_WS_STRING); return TOK_WORKSPACE; }
focus                           { return TOK_FOCUS; }
move                            { return TOK_MOVE; }
open                            { return TOK_OPEN; }
next                            { return TOK_NEXT; }
prev                            { return TOK_PREV; }
split                           { return TOK_SPLIT; }
horizontal                      { return TOK_HORIZONTAL; }
vertical                        { return TOK_VERTICAL; }
level                           { return TOK_LEVEL; }
up                              { return TOK_UP; }
down                            { return TOK_DOWN; }
left                            { return TOK_LEFT; }
right                           { return TOK_RIGHT; }
before                          { return TOK_BEFORE; }
after                           { return TOK_AFTER; }
resize                          { return TOK_RESIZE; }
shrink                          { return TOK_SHRINK; }
grow                            { return TOK_GROW; }
px                              { return TOK_PX; }
or                              { return TOK_OR; }
ppt                             { return TOK_PPT; }
restore                         { BEGIN(WANT_WS_STRING); return TOK_RESTORE; }
mark                            { BEGIN(WANT_WS_STRING); return TOK_MARK; }

class                           { BEGIN(WANT_QSTRING); return TOK_CLASS; }
id                              { BEGIN(WANT_QSTRING); return TOK_ID; }
con_id                          { BEGIN(WANT_QSTRING); return TOK_CON_ID; }
con_mark                        { BEGIN(WANT_QSTRING); return TOK_MARK; }

[0-9]+                          { cmdyylval.number = atoi(yytext); return NUMBER; }

.                               { return (int)yytext[0]; }

<<EOF>> {
    while (yy_start_stack_ptr > 0)
        yy_pop_state();
    yyterminate();
}

%%
