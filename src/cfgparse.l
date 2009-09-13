%{
#include <stdio.h>
#include "cfgparse.tab.h"
#include <xcb/xcb.h>

#include "data.h"

%}

%Start BIND_COND BIND_AWS_COND BIND_A2WS_COND ASSIGN_COND COLOR_COND

%%
<BIND_A2WS_COND>[^\n]+	{ BEGIN(INITIAL); yylval.string = strdup(yytext); return STR; }
#[^\n]*			return TOKCOMMENT;
[0-9]+                  yylval.number = atoi(yytext); return NUMBER;
<COLOR_COND>[0-9a-fA-F]+		{ yylval.string = strdup(yytext); return HEX; }
bind                    { BEGIN(BIND_COND); return TOKBIND; }
bindsym                 { BEGIN(BIND_COND); return TOKBINDSYM; }
floating_modifier	return TOKFLOATING_MODIFIER;
workspace		return TOKWORKSPACE;
screen			return TOKSCREEN;
terminal		{ BEGIN(BIND_AWS_COND); return TOKTERMINAL; }
font			{ BEGIN(BIND_AWS_COND); return TOKFONT; }
assign			{ BEGIN(ASSIGN_COND); return TOKASSIGN; }
set			{ BEGIN(BIND_COND); return TOKSET; }
ipc-socket		{ BEGIN(BIND_AWS_COND); return TOKIPCSOCKET; }
exec			{ BEGIN(BIND_AWS_COND); return TOKEXEC; }
client.focused		{ BEGIN(COLOR_COND); /* TODO: lval pointer */ return TOKCOLOR; }
Mod1			yylval.number = BIND_MOD1; return MODIFIER;
Mod2			yylval.number = BIND_MOD2; return MODIFIER;
Mod3			yylval.number = BIND_MOD3; return MODIFIER;
Mod4			yylval.number = BIND_MOD4; return MODIFIER;
Mod5			yylval.number = BIND_MOD5; return MODIFIER;
control			return TOKCONTROL;
shift			return TOKSHIFT;
→			return TOKARROW;
\n                      /* ignore end of line */;
<BIND_COND>[ \t]+       { BEGIN(BIND_AWS_COND); return WHITESPACE; }
<BIND_AWS_COND>[ \t]+   { BEGIN(BIND_A2WS_COND); return WHITESPACE; }
[ \t]+                  return WHITESPACE;
<ASSIGN_COND>[^ \t]+	{ BEGIN(INITIAL); yylval.string = strdup(yytext); return STR_NG; }
[a-zA-Z]+		yylval.string = strdup(yytext); return WORD;
[a-zA-Z0-9_-]+		yylval.string = strdup(yytext); return VARNAME;
\"[^\"]+\"		{
				/* yylval will be the string, but without quotes */
				char *copy = strdup(yytext+1);
				copy[strlen(copy)-1] = '\0';
				yylval.string = copy;
				return QUOTEDSTRING;
			}
.               return (int) yytext[0];
%%
