%{
/*
 * vim:ts=4:sw=4:expandtab
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>

#include "libi3.h"

extern Display *dpy;

struct context {
        int line_number;
        char *line_copy;

        char *compact_error;

        /* These are the same as in YYLTYPE */
        int first_column;
        int last_column;

        char *result;
};

typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern int yylex(struct context *context);
extern int yyparse(void);
extern FILE *yyin;
YY_BUFFER_STATE yy_scan_string(const char *);

static struct context *context;

/* We don’t need yydebug for now, as we got decent error messages using
 * yyerror(). Should you ever want to extend the parser, it might be handy
 * to just comment it in again, so it stays here. */
//int yydebug = 1;

void yyerror(const char *error_message) {
    fprintf(stderr, "\n");
    fprintf(stderr, "CONFIG: %s\n", error_message);
    fprintf(stderr, "CONFIG: line %d:\n",
        context->line_number);
    fprintf(stderr, "CONFIG:   %s\n", context->line_copy);
    fprintf(stderr, "CONFIG:   ");
    for (int c = 1; c <= context->last_column; c++)
        if (c >= context->first_column)
            fprintf(stderr, "^");
        else fprintf(stderr, " ");
    fprintf(stderr, "\n");
    fprintf(stderr, "\n");
}

int yywrap() {
    return 1;
}

char *rewrite_binding(const char *bindingline) {
    char *result = NULL;

    context = calloc(sizeof(struct context), 1);

    yy_scan_string(bindingline);

    if (yyparse() != 0) {
        fprintf(stderr, "Could not parse configfile\n");
        exit(1);
    }

    result = context->result;

    if (context->line_copy)
        free(context->line_copy);
    free(context);

    return result;
}

/* XXX: does not work for combinations of modifiers yet */
static char *modifier_to_string(int modifiers) {
    //printf("should convert %d to string\n", modifiers);
    if (modifiers == (1 << 3))
        return strdup("$mod+");
    else if (modifiers == ((1 << 3) | (1 << 0)))
        return strdup("$mod+Shift+");
    else if (modifiers == (1 << 9))
        return strdup("$mod+");
    else if (modifiers == ((1 << 9) | (1 << 0)))
        return strdup("$mod+Shift+");
    else if (modifiers == (1 << 0))
        return strdup("Shift+");
    else return strdup("");
}

%}

%error-verbose
%lex-param { struct context *context }

%union {
    int number;
    char *string;
}

%token <number>NUMBER "<number>"
%token <string>STR "<string>"
%token TOKBINDCODE
%token TOKMODVAR "$mod"
%token MODIFIER "<modifier>"
%token TOKCONTROL "control"
%token TOKSHIFT "shift"
%token WHITESPACE "<whitespace>"

%%

lines: /* empty */
    | lines WHITESPACE bindcode
    | lines error
    | lines bindcode
    ;

bindcode:
    TOKBINDCODE WHITESPACE binding_modifiers NUMBER WHITESPACE STR
    {
        //printf("\tFound keycode binding mod%d with key %d and command %s\n", $<number>3, $4, $<string>6);
        int level = 0;
        if (($<number>3 & (1 << 0))) {
            /* When shift is included, we really need to use the second-level
             * symbol (upper-case). The lower-case symbol could be on a
             * different key than the upper-case one (unlikely for letters, but
             * more likely for special characters). */
            level = 1;
        }
        KeySym sym = XKeycodeToKeysym(dpy, $4, level);
        char *str = XKeysymToString(sym);
        char *modifiers = modifier_to_string($<number>3);
        // TODO: modifier to string
        sasprintf(&(context->result), "bindsym %s%s %s\n", modifiers, str, $<string>6);
        free(modifiers);
    }
    ;

binding_modifiers:
    /* NULL */                               { $<number>$ = 0; }
    | binding_modifier
    | binding_modifiers '+' binding_modifier { $<number>$ = $<number>1 | $<number>3; }
    | binding_modifiers '+'                  { $<number>$ = $<number>1; }
    ;

binding_modifier:
    MODIFIER        { $<number>$ = $<number>1; }
    | TOKMODVAR     { $<number>$ = $<number>1; }
    | TOKCONTROL    { $<number>$ = (1 << 2); }
    | TOKSHIFT      { $<number>$ = (1 << 0); }
    ;
