%{
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2010 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * cmdparse.y: the parser for commands you send to i3 (or bind on keys)
 *

 */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "all.h"

typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern int cmdyylex(struct context *context);
extern int cmdyyparse(void);
extern FILE *cmdyyin;
YY_BUFFER_STATE cmdyy_scan_string(const char *);

static struct bindings_head *current_bindings;
static struct context *context;
static Match current_match;

/*
 * Helper data structure for an operation window (window on which the operation
 * will be performed). Used to build the TAILQ owindows.
 *
 */
typedef struct owindow {
    Con *con;
    TAILQ_ENTRY(owindow) owindows;
} owindow;
static TAILQ_HEAD(owindows_head, owindow) owindows;

/* We don’t need yydebug for now, as we got decent error messages using
 * yyerror(). Should you ever want to extend the parser, it might be handy
 * to just comment it in again, so it stays here. */
//int cmdyydebug = 1;

void cmdyyerror(const char *error_message) {
    ELOG("\n");
    ELOG("CMD: %s\n", error_message);
    ELOG("CMD: in file \"%s\", line %d:\n",
            context->filename, context->line_number);
    ELOG("CMD:   %s\n", context->line_copy);
    ELOG("CMD:   ");
    for (int c = 1; c <= context->last_column; c++)
        if (c >= context->first_column)
                printf("^");
        else printf(" ");
    printf("\n");
    ELOG("\n");
}

int cmdyywrap() {
    return 1;
}

void parse_cmd(const char *new) {

    //const char *new = "[level-up workspace] attach $output, focus";

    cmdyy_scan_string(new);

    context = scalloc(sizeof(struct context));
    context->filename = "cmd";
    if (cmdyyparse() != 0) {
            fprintf(stderr, "Could not parse configfile\n");
            exit(1);
    }
    printf("done\n");

    FREE(context->line_copy);
    free(context);
}

%}

%error-verbose
%lex-param { struct context *context }

%union {
    char *string;
}

%token TOK_ATTACH "attach"
%token TOK_EXEC "exec"
%token TOK_EXIT "exit"
%token TOK_RELOAD "reload"
%token TOK_RESTART "restart"
%token TOK_KILL "kill"
%token TOK_FULLSCREEN "fullscreen"
%token TOK_GLOBAL "global"
%token TOK_LAYOUT "layout"
%token TOK_DEFAULT "default"
%token TOK_STACKED "stacked"
%token TOK_TABBED "tabbed"
%token TOK_BORDER "border"
%token TOK_NONE "none"
%token TOK_1PIXEL "1pixel"
%token TOK_MODE "mode"
%token TOK_TILING "tiling"
%token TOK_FLOATING "floating"
%token TOK_WORKSPACE "workspace"
%token TOK_FOCUS "focus"
%token TOK_MOVE "move"
%token TOK_OPEN "open"

%token TOK_CLASS "class"
%token TOK_ID "id"
%token TOK_CON_ID "con_id"

%token WHITESPACE "<whitespace>"
%token STR "<string>"

%%

commands: /* empty */
    | commands optwhitespace ';' optwhitespace command
    | command
    {
        owindow *current;

        printf("single command completely parsed, dropping state...\n");
        while (!TAILQ_EMPTY(&owindows)) {
            current = TAILQ_FIRST(&owindows);
            TAILQ_REMOVE(&owindows, current, owindows);
            free(current);
        }
        memset(&current_match, 0, sizeof(Match));
    }
    ;

optwhitespace:
    | WHITESPACE
    ;

command:
    match optwhitespace operations
    ;

match:
    | matchstart optwhitespace criteria optwhitespace matchend
    {
        printf("match parsed\n");
    }
    ;

matchstart:
    '['
    {
        printf("start\n");
        memset(&current_match, '\0', sizeof(Match));
        TAILQ_INIT(&owindows);
        /* copy all_cons */
        Con *con;
        TAILQ_FOREACH(con, &all_cons, all_cons) {
            owindow *ow = smalloc(sizeof(owindow));
            ow->con = con;
            TAILQ_INSERT_TAIL(&owindows, ow, owindows);
        }
    }
    ;

matchend:
    ']'
    {
        owindow *next, *current;

        printf("match specification finished, matching...\n");
        /* copy the old list head to iterate through it and start with a fresh
         * list which will contain only matching windows */
        struct owindows_head old = owindows;
        TAILQ_INIT(&owindows);
        for (next = TAILQ_FIRST(&old); next != TAILQ_END(&old);) {
            /* make a copy of the next pointer and advance the pointer to the
             * next element as we are going to invalidate the element’s
             * next/prev pointers by calling TAILQ_INSERT_TAIL later */
            current = next;
            next = TAILQ_NEXT(next, owindows);

            printf("checking if con %p / %s matches\n", current->con, current->con->name);
            if (current_match.con_id != NULL) {
                if (current_match.con_id == current->con) {
                    printf("matches container!\n");
                    TAILQ_INSERT_TAIL(&owindows, current, owindows);

                }
            } else {
                if (current->con->window == NULL)
                    continue;
                if (match_matches_window(&current_match, current->con->window)) {
                    printf("matches window!\n");
                    TAILQ_INSERT_TAIL(&owindows, current, owindows);
                } else {
                    printf("doesnt match\n");
                    free(current);
                }
            }
        }

        TAILQ_FOREACH(current, &owindows, owindows) {
            printf("matching: %p / %s\n", current->con, current->con->name);
        }

    }
    ;

criteria:
    TOK_CLASS '=' STR
    {
        printf("criteria: class = %s\n", $<string>3);
        current_match.class = $<string>3;
    }
    | TOK_CON_ID '=' STR
    {
        printf("criteria: id = %s\n", $<string>3);
        /* TODO: correctly parse number */
        current_match.con_id = atoi($<string>3);
        printf("id as int = %d\n", current_match.con_id);
    }
    ;

operations:
    operation
    | operation optwhitespace
    | operations ',' optwhitespace operation
    ;

operation:
    exec
    | exit
    /*| reload
    | restart
    | mark
    | fullscreen
    | layout
    | border
    | mode
    | workspace
    | move*/
    | workspace
    | attach
    | focus
    | kill
    | open
    ;

exec:
    TOK_EXEC WHITESPACE STR
    {
        printf("should execute %s\n", $<string>3);
    }
    ;

exit:
    TOK_EXIT
    {
        printf("exit, bye bye\n");
    }
    ;

attach:
    TOK_ATTACH
    {
        printf("should attach\n");
    }
    ;

focus:
    TOK_FOCUS
    {
        printf("should focus\n");
    }
    ;

kill:
    TOK_KILL
    {
        owindow *current;

        printf("killing!\n");
        /* TODO: check if the match is empty, not if the result is empty */
        if (match_is_empty(&current_match))
            tree_close(focused);
        else {
        TAILQ_FOREACH(current, &owindows, owindows) {
            printf("matching: %p / %s\n", current->con, current->con->name);
            tree_close(current->con);
        }
        }

    }
    ;

workspace:
    TOK_WORKSPACE WHITESPACE STR
    {
        printf("should switch to workspace %s\n", $<string>3);
        workspace_show($<string>3);
        free($<string>3);
    }
    ;

open:
    TOK_OPEN
    {
        printf("opening new container\n");
        tree_open_con(NULL);
    }
    ;
