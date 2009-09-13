%{
#include <stdio.h>
#include <string.h>
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


int yydebug = 1;

void yyerror(const char *str)
{
        fprintf(stderr,"error: %s\n",str);
}

int yywrap()
{
        return 1;
}

#if 0
main()
{
        yyparse();
	printf("parsing done\n");
}
#endif

%}

%union {
	int number;
	char *string;
}

%token <number>NUMBER
%token <string>WORD
%token <string>STRING
%token <string>STRING_NG
%token <string>VARNAME
%token <string>HEX
%token TOKBIND
%token TOKBINDSYM
%token MODIFIER
%token TOKCONTROL
%token TOKSHIFT
%token WHITESPACE
%token TOKFLOATING_MODIFIER
%token QUOTEDSTRING
%token TOKWORKSPACE
%token TOKSCREEN
%token TOKASSIGN
%token TOKSET
%token TOKIPCSOCKET
%token TOKEXEC
%token TOKCOLOR
%token TOKARROW

%%

lines: /* empty */
        | lines WHITESPACE line
	| lines line
        ;

line:
        bind
	| bindsym
	| floating_modifier
	| workspace
	| assign
	| set
	| ipcsocket
	| exec
	| color
        ;

command:
	STRING
	;

bind:
        TOKBIND WHITESPACE binding_modifiers '+' NUMBER WHITESPACE command
        {
                printf("\tFound binding mod%d with key %d and command %s\n", $<number>3, $5, $<string>7);
        }
        ;

bindsym:
        TOKBINDSYM WHITESPACE binding_modifiers '+' WORD WHITESPACE command
        {
                printf("\tFound symbolic mod%d with key %s and command %s\n", $<number>3, $5, $<string>7);
        }
        ;

floating_modifier:
	TOKFLOATING_MODIFIER WHITESPACE binding_modifiers
	{
		printf("\tfloating modifier %d\n", $<number>3);
	}
	;

workspace:
	TOKWORKSPACE WHITESPACE NUMBER WHITESPACE TOKSCREEN WHITESPACE screen
	{
		printf("\t workspace %d to screen %d\n", $<number>3, $<number>7);
	}
	| TOKWORKSPACE WHITESPACE NUMBER WHITESPACE TOKSCREEN WHITESPACE screen WHITESPACE QUOTEDSTRING
	{
		printf("\t quoted: %s\n", $<string>9);
	}
	;

screen:
	NUMBER
	;


assign:
	TOKASSIGN WHITESPACE window_class WHITESPACE optional_arrow NUMBER
	{
		printf("assignment of %s to %d\n", $<string>3, $<number>6);
	}
	;

window_class:
	QUOTEDSTRING
	| STRING_NG
	;

optional_arrow:
	/* NULL */
	| TOKARROW WHITESPACE
	;

set:
	TOKSET WHITESPACE variable WHITESPACE STRING
	{
		printf("set %s to %s\n", $<string>3, $<string>5);
	}
	;

variable:
	'$' WORD 	{ asprintf(&$<string>$, "$%s", $<string>2); }
	| '$' VARNAME 	{ asprintf(&$<string>$, "$%s", $<string>2); }
	;

ipcsocket:
	TOKIPCSOCKET WHITESPACE STRING
	{
		printf("ipc %s\n", $<string>3);
	}
	;

exec:
	TOKEXEC WHITESPACE STRING
	{
		printf("exec %s\n", $<string>3);
	}
	;

color:
	TOKCOLOR WHITESPACE '#' HEX
	{
		printf("color %s\n", $<string>4);
	}
	;


binding_modifiers:
	binding_modifier
	|
	binding_modifiers '+' binding_modifier
	{
		$<number>$ = $<number>1 | $<number>3;
	}
	;

binding_modifier:
	MODIFIER 	{ $<number>$ = $<number>1; }
	| TOKCONTROL 	{ $<number>$ = BIND_CONTROL; }
	| TOKSHIFT 	{ $<number>$ = BIND_SHIFT; }
	;
