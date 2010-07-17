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

/* Holds the JSON which will be returned via IPC or NULL for the default return
 * message */
static char *json_output;

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

char *parse_cmd(const char *new) {

    //const char *new = "[level-up workspace] attach $output, focus";

    cmdyy_scan_string(new);

    context = scalloc(sizeof(struct context));
    context->filename = "cmd";
    FREE(json_output);
    if (cmdyyparse() != 0) {
            fprintf(stderr, "Could not parse configfile\n");
            exit(1);
    }
    printf("done, json output = %s\n", json_output);

    FREE(context->line_copy);
    free(context);
    return json_output;
}

%}

%error-verbose
%lex-param { struct context *context }

%union {
    char *string;
    char chr;
    int number;
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
%token TOK_TOGGLE "toggle"
%token TOK_FOCUS "focus"
%token TOK_MOVE "move"
%token TOK_OPEN "open"
%token TOK_NEXT "next"
%token TOK_PREV "prev"
%token TOK_SPLIT "split"
%token TOK_HORIZONTAL "horizontal"
%token TOK_VERTICAL "vertical"
%token TOK_LEVEL "level"
%token TOK_UP "up"
%token TOK_DOWN "down"
%token TOK_LEFT "left"
%token TOK_RIGHT "right"
%token TOK_AFTER "after"
%token TOK_BEFORE "before"
%token TOK_RESTORE "restore"
%token TOK_MARK "mark"
%token TOK_RESIZE "resize"
%token TOK_GROW "grow"
%token TOK_SHRINK "shrink"
%token TOK_PX "px"
%token TOK_OR "or"
%token TOK_PPT "ppt"

%token TOK_CLASS "class"
%token TOK_ID "id"
%token TOK_CON_ID "con_id"

%token WHITESPACE "<whitespace>"
%token STR "<string>"
%token NUMBER "<number>"

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
            } else if (current_match.mark != NULL && current->con->mark != NULL &&
                    strcasecmp(current_match.mark, current->con->mark) == 0) {
                printf("match by mark\n");
                    TAILQ_INSERT_TAIL(&owindows, current, owindows);

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
    | TOK_ID '=' STR
    {
        printf("criteria: window id = %s\n", $<string>3);
        /* TODO: correctly parse number */
        current_match.id = atoi($<string>3);
        printf("window id as int = %d\n", current_match.id);
    }
    | TOK_MARK '=' STR
    {
        printf("criteria: mark = %s\n", $<string>3);
        current_match.mark = $<string>3;
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
    | restart
    | reload
    /*
    | border */
    | layout
    | restore
    | move
    | workspace
    | attach
    | focus
    | kill
    | open
    | fullscreen
    | next
    | prev
    | split
    | mode
    | level
    | mark
    | resize
    ;

exec:
    TOK_EXEC WHITESPACE STR
    {
        printf("should execute %s\n", $<string>3);
        start_application($<string>3);
    }
    ;

exit:
    TOK_EXIT
    {
        printf("exit, bye bye\n");
        exit(0);
    }
    ;

reload:
    TOK_RELOAD
    {
        printf("reloading\n");
        load_configuration(conn, NULL, true);
        /* Send an IPC event just in case the ws names have changed */
        ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"reload\"}");
    }
    ;

restart:
    TOK_RESTART
    {
        printf("restarting i3\n");
        i3_restart();
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
        owindow *current;

        printf("should focus\n");
        if (match_is_empty(&current_match)) {
            /* TODO: better error message */
            LOG("Error: The foucs command requires you to use some criteria.\n");
            return;
        }

        /* TODO: warning if the match contains more than one entry. does not
         * make so much sense when focusing */
        TAILQ_FOREACH(current, &owindows, owindows) {
            LOG("focusing %p / %s\n", current->con, current->con->name);
            con_focus(current->con);
        }
    }
    ;

kill:
    TOK_KILL
    {
        owindow *current;

        printf("killing!\n");
        /* check if the match is empty, not if the result is empty */
        if (match_is_empty(&current_match))
            tree_close_con();
        else {
            TAILQ_FOREACH(current, &owindows, owindows) {
                printf("matching: %p / %s\n", current->con, current->con->name);
                tree_close(current->con, true);
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
        Con *con = tree_open_con(NULL);
        asprintf(&json_output, "{\"success\":true, \"id\":%d}", (long int)con);
    }
    ;

fullscreen:
    TOK_FULLSCREEN
    {
        printf("toggling fullscreen\n");
        owindow *current;

        /* check if the match is empty, not if the result is empty */
        if (match_is_empty(&current_match))
            con_toggle_fullscreen(focused);
        else {
            TAILQ_FOREACH(current, &owindows, owindows) {
                printf("matching: %p / %s\n", current->con, current->con->name);
                con_toggle_fullscreen(current->con);
            }
        }

    }
    ;

next:
    TOK_NEXT WHITESPACE direction
    {
        /* TODO: use matches */
        printf("should select next window in direction %c\n", $<chr>3);
        tree_next('n', ($<chr>3 == 'v' ? VERT : HORIZ));
    }
    ;

prev:
    TOK_PREV WHITESPACE direction
    {
        /* TODO: use matches */
        printf("should select prev window in direction %c\n", $<chr>3);
        tree_next('p', ($<chr>3 == 'v' ? VERT : HORIZ));
    }
    ;

split:
    TOK_SPLIT WHITESPACE direction
    {
        /* TODO: use matches */
        printf("splitting in direction %c\n", $<chr>3);
        tree_split(focused, ($<chr>3 == 'v' ? VERT : HORIZ));
    }
    ;

direction:
    TOK_HORIZONTAL  { $<chr>$ = 'h'; }
    | 'h'           { $<chr>$ = 'h'; }
    | TOK_VERTICAL  { $<chr>$ = 'v'; }
    | 'v'           { $<chr>$ = 'v'; }
    ;

mode:
    TOK_MODE WHITESPACE window_mode
    {
        if ($<number>3 == TOK_TOGGLE) {
            printf("should toggle mode\n");
            toggle_floating_mode(focused, false);
        } else {
            printf("should switch mode to %s\n", ($<number>3 == TOK_FLOATING ? "floating" : "tiling"));
            if ($<number>3 == TOK_FLOATING) {
                floating_enable(focused, false);
            } else {
                floating_disable(focused, false);
            }
        }
    }
    ;

window_mode:
    TOK_FLOATING    { $<number>$ = TOK_FLOATING; }
    | TOK_TILING    { $<number>$ = TOK_TILING; }
    | TOK_TOGGLE    { $<number>$ = TOK_TOGGLE; }
    ;

level:
    TOK_LEVEL WHITESPACE level_direction
    {
        printf("level %c\n", $<chr>3);
        if ($<chr>3 == 'u')
            level_up();
        else level_down();
    }
    ;

level_direction:
    TOK_UP     { $<chr>$ = 'u'; }
    | TOK_DOWN { $<chr>$ = 'd'; }
    ;

move:
    TOK_MOVE WHITESPACE before_after WHITESPACE direction
    {
        printf("moving: %s and %c\n", ($<number>3 == TOK_BEFORE ? "before" : "after"), $<chr>5);
        /* TODO: change API for the next call, we need to convert in both directions while ideally
         * we should not need any of both */
        tree_move(($<number>3 == TOK_BEFORE ? 'p' : 'n'), ($<chr>5 == 'v' ? VERT : HORIZ));
    }
    | TOK_MOVE WHITESPACE TOK_WORKSPACE WHITESPACE STR
    {
        owindow *current;

        printf("should move window to workspace %s\n", $<string>5);
        /* get the workspace */
        Con *ws = workspace_get($<string>5);

        /* check if the match is empty, not if the result is empty */
        if (match_is_empty(&current_match))
            con_move_to_workspace(focused, ws);
        else {
            TAILQ_FOREACH(current, &owindows, owindows) {
                printf("matching: %p / %s\n", current->con, current->con->name);
                con_move_to_workspace(current->con, ws);
            }
        }
    }
    ;

before_after:
    TOK_BEFORE { $<number>$ = TOK_BEFORE; }
    | TOK_AFTER { $<number>$ = TOK_AFTER; }
    ;

restore:
    TOK_RESTORE WHITESPACE STR
    {
        printf("restoring \"%s\"\n", $<string>3);
        tree_append_json($<string>3);
    }
    ;

layout:
    TOK_LAYOUT WHITESPACE layout_mode
    {
        printf("changing layout to %d\n", $<number>3);
        owindow *current;

        /* check if the match is empty, not if the result is empty */
        if (match_is_empty(&current_match))
            focused->parent->layout = $<number>3;
        else {
            TAILQ_FOREACH(current, &owindows, owindows) {
                printf("matching: %p / %s\n", current->con, current->con->name);
                current->con->layout = $<number>3;
            }
        }

    }
    ;

layout_mode:
    TOK_DEFAULT   { $<number>$ = L_DEFAULT; }
    | TOK_STACKED { $<number>$ = L_STACKED; }
    | TOK_TABBED  { $<number>$ = L_TABBED; }
    ;

mark:
    TOK_MARK WHITESPACE STR
    {
        printf("marking window with str %s\n", $<string>3);
        owindow *current;

        /* check if the match is empty, not if the result is empty */
        if (match_is_empty(&current_match))
            focused->mark = sstrdup($<string>3);
        else {
            TAILQ_FOREACH(current, &owindows, owindows) {
                printf("matching: %p / %s\n", current->con, current->con->name);
                current->con->mark = sstrdup($<string>3);
            }
        }

        free($<string>3);
    }
    ;

resize:
    TOK_RESIZE WHITESPACE resize_way WHITESPACE direction resize_px resize_tiling
    {
        /* resize <grow|shrink> <direction> [<px> px] [or <ppt> ppt] */
        printf("resizing in way %d, direction %d, px %d or ppt %d\n", $<number>3, $<number>5, $<number>6, $<number>7);
        int direction = $<number>5;
        int px = $<number>6;
        int ppt = $<number>7;
        if ($<number>3 == TOK_SHRINK) {
            px *= -1;
            ppt *= -1;
        }

        if (con_is_floating(focused)) {
            printf("floating resize\n");
            if (direction == TOK_UP) {
                focused->parent->rect.y -= px;
                focused->parent->rect.height += px;
            } else if (direction == TOK_DOWN) {
                focused->rect.height += px;
            } else if (direction == TOK_LEFT) {
                focused->rect.x -= px;
                focused->rect.width += px;
            } else {
                focused->rect.width += px;
            }
        } else {
            LOG("tiling resize\n");
            /* get the default percentage */
            int children = 0;
            Con *other;
            TAILQ_FOREACH(other, &(focused->parent->nodes_head), nodes)
                children++;
            LOG("ins. %d children\n", children);
            double percentage = 1.0 / children;
            LOG("default percentage = %f\n", percentage);

            if (direction == TOK_UP || direction == TOK_LEFT) {
                other = TAILQ_PREV(focused, nodes_head, nodes);
            } else {
                other = TAILQ_NEXT(focused, nodes);
            }
            if (other == TAILQ_END(workspaces)) {
                LOG("No other container in this direction found, cannot resize.\n");
                return 0;
            }
            LOG("other->percent = %f\n", other->percent);
            LOG("focused->percent before = %f\n", focused->percent);
            if (focused->percent == 0.0)
                focused->percent = percentage;
            if (other->percent == 0.0)
                other->percent = percentage;
            focused->percent += ((double)ppt / 100.0);
            other->percent -= ((double)ppt / 100.0);
            LOG("focused->percent after = %f\n", focused->percent);
            LOG("other->percent after = %f\n", other->percent);
        }
    }
    ;

resize_px:
    /* empty */
    {
        $<number>$ = 10;
    }
    | WHITESPACE NUMBER WHITESPACE TOK_PX
    {
        $<number>$ = $<number>2;
    }
    ;

resize_tiling:
    /* empty */
    {
        $<number>$ = 10;
    }
    | WHITESPACE TOK_OR WHITESPACE NUMBER WHITESPACE TOK_PPT
    {
        $<number>$ = $<number>4;
    }
    ;

resize_way:
    TOK_GROW        { $<number>$ = TOK_GROW; }
    | TOK_SHRINK    { $<number>$ = TOK_SHRINK; }
    ;

direction:
    TOK_UP          { $<number>$ = TOK_UP; }
    | TOK_DOWN      { $<number>$ = TOK_DOWN; }
    | TOK_LEFT      { $<number>$ = TOK_LEFT; }
    | TOK_RIGHT     { $<number>$ = TOK_RIGHT; }
    ;
