%{
#include <stdio.h>
#include "cfgparse.tab.h"
#include <xcb/xcb.h>

enum {
        BIND_NONE = 0,
        BIND_SHIFT = XCB_MOD_MASK_SHIFT,        /* (1 << 0) */
        BIND_CONTROL = XCB_MOD_MASK_CONTROL,    /* (1 << 2) */
        BIND_MOD1 = XCB_MOD_MASK_1,             /* (1 << 3) */
        BIND_MOD2 = XCB_MOD_MASK_2,             /* (1 << 4) */
        BIND_MOD3 = XCB_MOD_MASK_3,             /* (1 << 5) */
        BIND_MOD4 = XCB_MOD_MASK_4,             /* (1 << 6) */
        BIND_MOD5 = XCB_MOD_MASK_5,             /* (1 << 7) */
        BIND_MODE_SWITCH = (1 << 8)
};

%}

%Start BIND_COND BIND_AWS_COND BIND_A2WS_COND ASSIGN_COND

%%
[0-9]+                  yylval.number = atoi(yytext); return NUMBER;
[0-9a-fA-F]+		{ yylval.string = strdup(yytext); return HEX; }
bind                    { BEGIN(BIND_COND); return TOKBIND; }
bindsym                 { BEGIN(BIND_COND); return TOKBINDSYM; }
floating_modifier	return TOKFLOATING_MODIFIER;
workspace		return TOKWORKSPACE;
screen			return TOKSCREEN;
assign			{ BEGIN(ASSIGN_COND); return TOKASSIGN; }
set			{ BEGIN(BIND_COND); return TOKSET; }
ipc-socket		{ BEGIN(BIND_AWS_COND); return TOKIPCSOCKET; }
exec			{ BEGIN(BIND_AWS_COND); return TOKEXEC; }
client.focused		{ /* TODO: lval pointer */ return TOKCOLOR; }
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
<BIND_A2WS_COND>[^\n]+	{ BEGIN(INITIAL); yylval.string = strdup(yytext); return STRING; }
<ASSIGN_COND>[^ \t]+	{ BEGIN(INITIAL); yylval.string = strdup(yytext); return STRING_NG; }
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
