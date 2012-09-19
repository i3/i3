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
#include <X11/XKBlib.h>

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
static xcb_connection_t *conn;
static xcb_key_symbols_t *keysyms;

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

    conn = xcb_connect(NULL, NULL);
    if (conn == NULL || xcb_connection_has_error(conn)) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }
    keysyms = xcb_key_symbols_alloc(conn);

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
    xcb_key_symbols_free(keysyms);
    xcb_disconnect(conn);

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

/*
 * Returns true if sym is bound to any key except for 'except_keycode' on the
 * first four layers (normal, shift, mode_switch, mode_switch + shift).
 *
 */
static bool keysym_used_on_other_key(KeySym sym, xcb_keycode_t except_keycode) {
    xcb_keycode_t i,
                  min_keycode = xcb_get_setup(conn)->min_keycode,
                  max_keycode = xcb_get_setup(conn)->max_keycode;

    for (i = min_keycode; i && i <= max_keycode; i++) {
        if (i == except_keycode)
            continue;
        for (int level = 0; level < 4; level++) {
            if (xcb_key_symbols_get_keysym(keysyms, i, level) != sym)
                continue;
            return true;
        }
    }
    return false;
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

            /* Try to use the keysym on the first level (lower-case). In case
             * this doesn’t make it ambiguous (think of a keyboard layout
             * having '1' on two different keys, but '!' only on keycode 10),
             * we’ll stick with the keysym of the first level.
             *
             * This reduces a lot of confusion for users who switch keyboard
             * layouts from qwerty to qwertz or other slight variations of
             * qwerty (yes, that happens quite often). */
            KeySym sym = XkbKeycodeToKeysym(dpy, $4, 0, 0);
            if (!keysym_used_on_other_key(sym, $4))
                level = 0;
        }
        KeySym sym = XkbKeycodeToKeysym(dpy, $4, 0, level);
        char *str = XKeysymToString(sym);
        char *modifiers = modifier_to_string($<number>3);
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
