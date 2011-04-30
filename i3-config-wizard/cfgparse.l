%option nounput
%option noinput
%option noyy_top_state
%option stack

%{
/*
 * vim:ts=8:expandtab
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "cfgparse.tab.h"

int yycolumn = 1;

struct context {
        int line_number;
        char *line_copy;

        char *compact_error;

        /* These are the same as in YYLTYPE */
        int first_column;
        int last_column;
};


#define YY_DECL int yylex (struct context *context)

#define YY_USER_ACTION { \
        context->first_column = yycolumn; \
        context->last_column = yycolumn+yyleng-1; \
        yycolumn += yyleng; \
}

%}

EOL	(\r?\n)

%s BINDCODE_COND
%s BIND_AWS_COND
%s BIND_A2WS_COND
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
	context->line_copy = strdup(yytext);

	yyless(0);
	yy_pop_state();
	yy_set_bol(true);
	yycolumn = 1;
}


<BIND_A2WS_COND>[^\n]+          { BEGIN(INITIAL); yylval.string = strdup(yytext); return STR; }
[0-9]+                          { yylval.number = atoi(yytext); return NUMBER; }
bind                            { BEGIN(BINDCODE_COND); return TOKBINDCODE; }
bindcode                        { BEGIN(BINDCODE_COND); return TOKBINDCODE; }
Mod1                            { yylval.number = (1 << 3); return MODIFIER; }
Mod2                            { yylval.number = (1 << 4); return MODIFIER; }
Mod3                            { yylval.number = (1 << 5); return MODIFIER; }
Mod4                            { yylval.number = (1 << 6); return MODIFIER; }
Mod5                            { yylval.number = (1 << 7); return MODIFIER; }
Mode_switch                     { yylval.number = (1 << 8); return MODIFIER; }
control                         { return TOKCONTROL; }
ctrl                            { return TOKCONTROL; }
shift                           { return TOKSHIFT; }
{EOL}                           {
				  if (context->line_copy) {
                                    free(context->line_copy);
				    context->line_copy = NULL;
				  }
                                  context->line_number++;
                                  BEGIN(INITIAL);
                                  yy_push_state(BUFFER_LINE);
                                }
<BINDCODE_COND>[ \t]+           { BEGIN(BIND_AWS_COND); return WHITESPACE; }
<BIND_AWS_COND>[ \t]+           { BEGIN(BIND_A2WS_COND); return WHITESPACE; }
[ \t]+                          { return WHITESPACE; }
.                               { return (int)yytext[0]; }

<<EOF>> {
        while (yy_start_stack_ptr > 0)
                yy_pop_state();
        yyterminate();
}

%%
