%{
#include <stdio.h>
#include "cfgparse.tab.h"
#include <xcb/xcb.h>

#include "data.h"
#include "config.h"

%}

%Start BIND_COND
%Start BIND_AWS_COND
%Start BIND_A2WS_COND
%Start ASSIGN_COND
%Start COLOR_COND
%Start SET_COND
%Start SET_AWS_COND
%Start SCREEN_COND
%Start SCREEN_AWS_COND

%%
<BIND_A2WS_COND>[^\n]+          { BEGIN(INITIAL); yylval.string = strdup(yytext); return STR; }
^#[^\n]*                        { return TOKCOMMENT; }
[0-9]+                          { yylval.number = atoi(yytext); return NUMBER; }
<COLOR_COND>[0-9a-fA-F]+        { yylval.string = strdup(yytext); return HEX; }
bind                            { BEGIN(BIND_COND); return TOKBIND; }
bindsym                         { BEGIN(BIND_COND); return TOKBINDSYM; }
floating_modifier               { return TOKFLOATING_MODIFIER; }
workspace                       { BEGIN(INITIAL); return TOKWORKSPACE; }
screen                          { BEGIN(SCREEN_COND); return TOKSCREEN; }
terminal                        { BEGIN(BIND_AWS_COND); return TOKTERMINAL; }
font                            { BEGIN(BIND_AWS_COND); return TOKFONT; }
assign                          { BEGIN(ASSIGN_COND); return TOKASSIGN; }
set                             { BEGIN(SET_COND); return TOKSET; }
ipc-socket                      { BEGIN(BIND_AWS_COND); return TOKIPCSOCKET; }
exec                            { BEGIN(BIND_AWS_COND); return TOKEXEC; }
client.focused                  { BEGIN(COLOR_COND); yylval.color = &config.client.focused; return TOKCOLOR; }
client.focused_inactive         { BEGIN(COLOR_COND); yylval.color = &config.client.focused_inactive; return TOKCOLOR; }
client.unfocused                { BEGIN(COLOR_COND); yylval.color = &config.client.unfocused; return TOKCOLOR; }
client.urgent                   { BEGIN(COLOR_COND); yylval.color = &config.client.urgent; return TOKCOLOR; }
bar.focused                     { BEGIN(COLOR_COND); yylval.color = &config.client.focused; return TOKCOLOR; }
bar.unfocused                   { BEGIN(COLOR_COND); yylval.color = &config.client.unfocused; return TOKCOLOR; }
bar.urgent                      { BEGIN(COLOR_COND); yylval.color = &config.client.urgent; return TOKCOLOR; }
Mod1                            { yylval.number = BIND_MOD1; return MODIFIER; }
Mod2                            { yylval.number = BIND_MOD2; return MODIFIER; }
Mod3                            { yylval.number = BIND_MOD3; return MODIFIER; }
Mod4                            { yylval.number = BIND_MOD4; return MODIFIER; }
Mod5                            { yylval.number = BIND_MOD5; return MODIFIER; }
control                         { return TOKCONTROL; }
shift                           { return TOKSHIFT; }
→                               { return TOKARROW; }
\n                              /* ignore end of line */;
<SCREEN_AWS_COND>x              { return (int)yytext[0]; }
<BIND_COND>[ \t]+               { BEGIN(BIND_AWS_COND); return WHITESPACE; }
<SET_COND>[ \t]+                { BEGIN(SET_AWS_COND); return WHITESPACE; }
<BIND_AWS_COND>[ \t]+           { BEGIN(BIND_A2WS_COND); return WHITESPACE; }
<SET_AWS_COND>[ \t]+            { BEGIN(BIND_A2WS_COND); return WHITESPACE; }
<SCREEN_COND>[ \t]+             { BEGIN(SCREEN_AWS_COND); return WHITESPACE; }
<SCREEN_AWS_COND>[ \t]+         { BEGIN(BIND_A2WS_COND); return WHITESPACE; }
[ \t]+                          { return WHITESPACE; }
\"[^\"]+\"                      {
                                  /* if ASSIGN_COND then */
                                  BEGIN(INITIAL);
                                  /* yylval will be the string, but without quotes */
                                  char *copy = strdup(yytext+1);
                                  copy[strlen(copy)-1] = '\0';
                                  yylval.string = copy;
                                  return QUOTEDSTRING;
                                }
<ASSIGN_COND>[^ \t]+            { BEGIN(INITIAL); yylval.string = strdup(yytext); return STR_NG; }
[a-zA-Z]+                       { yylval.string = strdup(yytext); return WORD; }
<SET_AWS_COND>[a-zA-Z0-9_-]+    { yylval.string = strdup(yytext); return VARNAME; }
.                               { return (int)yytext[0]; }
%%
