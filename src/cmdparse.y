%{
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * cmdparse.y: the parser for commands you send to i3 (or bind on keys)
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <float.h>

#include "all.h"

typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern int cmdyylex(struct context *context);
extern int cmdyyparse(void);
extern int cmdyylex_destroy(void);
extern FILE *cmdyyin;
YY_BUFFER_STATE cmdyy_scan_string(const char *);

static struct context *context;
static Match current_match;

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
    ELOG("CMD: in command:\n");
    ELOG("CMD:   %s\n", context->line_copy);
    ELOG("CMD:   ");
    for (int c = 1; c <= context->last_column; c++)
        if (c >= context->first_column)
                printf("^");
        else printf(" ");
    printf("\n");
    ELOG("\n");
    context->compact_error = sstrdup(error_message);
}

int cmdyywrap() {
    return 1;
}

char *parse_cmd(const char *new) {
    json_output = NULL;
    LOG("COMMAND: *%s*\n", new);
    cmdyy_scan_string(new);

    cmd_criteria_init(&current_match);
    context = scalloc(sizeof(struct context));
    context->filename = "cmd";
    if (cmdyyparse() != 0) {
        fprintf(stderr, "Could not parse command\n");
        sasprintf(&json_output, "{\"success\":false, \"error\":\"%s at position %d\"}",
                  context->compact_error, context->first_column);
        FREE(context->line_copy);
        FREE(context->compact_error);
        free(context);
        return json_output;
    }
    printf("done, json output = %s\n", json_output);

    cmdyylex_destroy();
    FREE(context->line_copy);
    FREE(context->compact_error);
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

%token              TOK_EXEC            "exec"
%token              TOK_EXIT            "exit"
%token              TOK_RELOAD          "reload"
%token              TOK_RESTART         "restart"
%token              TOK_KILL            "kill"
%token              TOK_WINDOW          "window"
%token              TOK_CONTAINER       "container"
%token              TOK_CLIENT          "client"
%token              TOK_FULLSCREEN      "fullscreen"
%token              TOK_GLOBAL          "global"
%token              TOK_LAYOUT          "layout"
%token              TOK_DEFAULT         "default"
%token              TOK_STACKED         "stacked"
%token              TOK_TABBED          "tabbed"
%token              TOK_BORDER          "border"
%token              TOK_NORMAL          "normal"
%token              TOK_NONE            "none"
%token              TOK_1PIXEL          "1pixel"
%token              TOK_MODE            "mode"
%token              TOK_TILING          "tiling"
%token              TOK_FLOATING        "floating"
%token              TOK_MODE_TOGGLE     "mode_toggle"
%token              TOK_ENABLE          "enable"
%token              TOK_DISABLE         "disable"
%token              TOK_WORKSPACE       "workspace"
%token              TOK_OUTPUT          "output"
%token              TOK_TOGGLE          "toggle"
%token              TOK_FOCUS           "focus"
%token              TOK_MOVE            "move"
%token              TOK_OPEN            "open"
%token              TOK_NEXT            "next"
%token              TOK_PREV            "prev"
%token              TOK_NEXT_ON_OUTPUT  "next_on_output"
%token              TOK_PREV_ON_OUTPUT  "prev_on_output"
%token              TOK_SCRATCHPAD      "scratchpad"
%token              TOK_SHOW            "show"
%token              TOK_SPLIT           "split"
%token              TOK_HORIZONTAL      "horizontal"
%token              TOK_VERTICAL        "vertical"
%token              TOK_UP              "up"
%token              TOK_DOWN            "down"
%token              TOK_LEFT            "left"
%token              TOK_RIGHT           "right"
%token              TOK_PARENT          "parent"
%token              TOK_CHILD           "child"
%token              TOK_APPEND_LAYOUT   "append_layout"
%token              TOK_MARK            "mark"
%token              TOK_RESIZE          "resize"
%token              TOK_GROW            "grow"
%token              TOK_SHRINK          "shrink"
%token              TOK_PX              "px"
%token              TOK_OR              "or"
%token              TOK_PPT             "ppt"
%token              TOK_NOP             "nop"
%token              TOK_BACK_AND_FORTH  "back_and_forth"
%token              TOK_NO_STARTUP_ID   "--no-startup-id"
%token              TOK_TO              "to"

%token              TOK_CLASS           "class"
%token              TOK_INSTANCE        "instance"
%token              TOK_WINDOW_ROLE     "window_role"
%token              TOK_ID              "id"
%token              TOK_CON_ID          "con_id"
%token              TOK_TITLE           "title"

%token  <string>    STR                 "<string>"
%token  <number>    NUMBER              "<number>"

%type   <number>    direction
%type   <number>    split_direction
%type   <number>    fullscreen_mode
%type   <number>    level
%type   <number>    window_mode
%type   <number>    boolean
%type   <number>    border_style
%type   <number>    layout_mode
%type   <number>    resize_px
%type   <number>    resize_way
%type   <number>    resize_tiling
%type   <number>    optional_kill_mode
%type   <number>    optional_no_startup_id

%%

commands:
    commands ';' command
    | command
    {
        cmd_criteria_init(&current_match);
    }
    ;

command:
    match operations
    ;

match:
    | matchstart criteria matchend
    ;

matchstart:
    '['
    ;

matchend:
    ']'
    {
        json_output = cmd_criteria_match_windows(&current_match);
    }
    ;

criteria:
    criteria criterion
    | criterion
    ;

criterion:
    TOK_CLASS '=' STR
    {
        cmd_criteria_add(&current_match, "class", $3);
        free($3);
    }
    | TOK_INSTANCE '=' STR
    {
        cmd_criteria_add(&current_match, "instance", $3);
        free($3);
    }
    | TOK_WINDOW_ROLE '=' STR
    {
        cmd_criteria_add(&current_match, "window_role", $3);
        free($3);
    }
    | TOK_CON_ID '=' STR
    {
        cmd_criteria_add(&current_match, "con_id", $3);
        free($3);
    }
    | TOK_ID '=' STR
    {
        cmd_criteria_add(&current_match, "id", $3);
        free($3);
    }
    | TOK_MARK '=' STR
    {
        cmd_criteria_add(&current_match, "con_mark", $3);
        free($3);
    }
    | TOK_TITLE '=' STR
    {
        cmd_criteria_add(&current_match, "title", $3);
        free($3);
    }
    ;

operations:
    operation
    | operations ',' operation
    ;

operation:
    exec
    | exit
    | restart
    | reload
    | border
    | layout
    | append_layout
    | move
    | workspace
    | focus
    | kill
    | open
    | fullscreen
    | split
    | floating
    | mark
    | resize
    | nop
    | scratchpad
    | mode
    ;

exec:
    TOK_EXEC optional_no_startup_id STR
    {
        json_output = cmd_exec(&current_match, ($2 ? "nosn" : NULL), $3);
        free($3);
    }
    ;

optional_no_startup_id:
    /* empty */ { $$ = false; }
    | TOK_NO_STARTUP_ID  { $$ = true; }
    ;

exit:
    TOK_EXIT
    {
        json_output = cmd_exit(&current_match);
    }
    ;

reload:
    TOK_RELOAD
    {
        json_output = cmd_reload(&current_match);
    }
    ;

restart:
    TOK_RESTART
    {
        json_output = cmd_restart(&current_match);
    }
    ;

focus:
    TOK_FOCUS
    {
        json_output = cmd_focus(&current_match);
    }
    | TOK_FOCUS direction
    {
        json_output = cmd_focus_direction(&current_match,
                            ($2 == TOK_LEFT ? "left" :
                             ($2 == TOK_RIGHT ? "right" :
                              ($2 == TOK_UP ? "up" :
                               "down"))));
    }
    | TOK_FOCUS TOK_OUTPUT STR
    {
        json_output = cmd_focus_output(&current_match, $3);
        free($3);
    }
    | TOK_FOCUS window_mode
    {

        json_output = cmd_focus_window_mode(&current_match,
                              ($2 == TOK_TILING ? "tiling" :
                               ($2 == TOK_FLOATING ? "floating" :
                                "mode_toggle")));
    }
    | TOK_FOCUS level
    {
        json_output = cmd_focus_level(&current_match, ($2 == TOK_PARENT ? "parent" : "child"));
    }
    ;

window_mode:
    TOK_TILING        { $$ = TOK_TILING; }
    | TOK_FLOATING    { $$ = TOK_FLOATING; }
    | TOK_MODE_TOGGLE { $$ = TOK_MODE_TOGGLE; }
    ;

level:
    TOK_PARENT  { $$ = TOK_PARENT; }
    | TOK_CHILD { $$ = TOK_CHILD;  }
    ;

kill:
    TOK_KILL optional_kill_mode
    {
        json_output = cmd_kill(&current_match, ($2 == KILL_WINDOW ? "window" : "client"));
    }
    ;

optional_kill_mode:
    /* empty */             { $$ = KILL_WINDOW; }
    | TOK_WINDOW { $$ = KILL_WINDOW; }
    | TOK_CLIENT { $$ = KILL_CLIENT; }
    ;

workspace:
    TOK_WORKSPACE TOK_NEXT
    {
        json_output = cmd_workspace(&current_match, "next");
    }
    | TOK_WORKSPACE TOK_PREV
    {
        json_output = cmd_workspace(&current_match, "prev");
    }
    | TOK_WORKSPACE TOK_NEXT_ON_OUTPUT
    {
        json_output = cmd_workspace(&current_match, "next_on_output");
    }
    | TOK_WORKSPACE TOK_PREV_ON_OUTPUT
    {
        json_output = cmd_workspace(&current_match, "prev_on_output");
    }
    | TOK_WORKSPACE TOK_BACK_AND_FORTH
    {
        json_output = cmd_workspace_back_and_forth(&current_match);
    }
    | TOK_WORKSPACE STR
    {
        json_output = cmd_workspace_name(&current_match, $2);
        free($2);
    }
    ;

open:
    TOK_OPEN
    {
        json_output = cmd_open(&current_match);
    }
    ;

fullscreen:
    TOK_FULLSCREEN fullscreen_mode
    {
        json_output = cmd_fullscreen(&current_match, ($2 == CF_OUTPUT ? "output" : "global"));
    }
    ;

fullscreen_mode:
    /* empty */  { $$ = CF_OUTPUT; }
    | TOK_GLOBAL { $$ = CF_GLOBAL; }
    ;

split:
    TOK_SPLIT split_direction
    {
        char buf[2] = {'\0', '\0'};
        buf[0] = $2;
        json_output = cmd_split(&current_match, buf);
    }
    ;

split_direction:
    TOK_HORIZONTAL  { $$ = 'h'; }
    | 'h'           { $$ = 'h'; }
    | TOK_VERTICAL  { $$ = 'v'; }
    | 'v'           { $$ = 'v'; }
    ;

floating:
    TOK_FLOATING boolean
    {
        json_output = cmd_floating(&current_match,
                     ($2 == TOK_ENABLE ? "enable" :
                      ($2 == TOK_DISABLE ? "disable" :
                       "toggle")));
    }
    ;

boolean:
    TOK_ENABLE    { $$ = TOK_ENABLE; }
    | TOK_DISABLE { $$ = TOK_DISABLE; }
    | TOK_TOGGLE  { $$ = TOK_TOGGLE; }
    ;

border:
    TOK_BORDER border_style
    {
        json_output = cmd_border(&current_match,
                   ($2 == BS_NORMAL ? "normal" :
                    ($2 == BS_NONE ? "none" :
                     ($2 == BS_1PIXEL ? "1pixel" :
                      "toggle"))));
    }
    ;

border_style:
    TOK_NORMAL      { $$ = BS_NORMAL; }
    | TOK_NONE      { $$ = BS_NONE; }
    | TOK_1PIXEL    { $$ = BS_1PIXEL; }
    | TOK_TOGGLE    { $$ = TOK_TOGGLE; }
    ;

move:
    TOK_MOVE direction resize_px
    {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "%d", $3);
        json_output = cmd_move_direction(&current_match,
                           ($2 == TOK_LEFT ? "left" :
                            ($2 == TOK_RIGHT ? "right" :
                             ($2 == TOK_UP ? "up" :
                              "down"))),
                           buffer);
    }
    | TOK_MOVE TOK_WORKSPACE STR
    {
        json_output = cmd_move_con_to_workspace_name(&current_match, $3);
    }
    | TOK_MOVE TOK_WORKSPACE TOK_NEXT
    {
        json_output = cmd_move_con_to_workspace(&current_match, "next");
    }
    | TOK_MOVE TOK_WORKSPACE TOK_PREV
    {
        json_output = cmd_move_con_to_workspace(&current_match, "prev");
    }
    | TOK_MOVE TOK_WORKSPACE TOK_NEXT_ON_OUTPUT
    {
        json_output = cmd_move_con_to_workspace(&current_match, "next_on_output");
    }
    | TOK_MOVE TOK_WORKSPACE TOK_PREV_ON_OUTPUT
    {
        json_output = cmd_move_con_to_workspace(&current_match, "prev_on_output");
    }
    | TOK_MOVE TOK_OUTPUT STR
    {
        json_output = cmd_move_con_to_output(&current_match, $3);
        free($3);
    }
    | TOK_MOVE TOK_SCRATCHPAD
    {
        json_output = cmd_move_scratchpad(&current_match);
    }
    | TOK_MOVE TOK_WORKSPACE TOK_TO TOK_OUTPUT STR
    {
        json_output = cmd_move_workspace_to_output(&current_match, $5);
        free($5);
    }
    ;

append_layout:
    TOK_APPEND_LAYOUT STR
    {
        json_output = cmd_append_layout(&current_match, $2);
        free($2);
    }
    ;

layout:
    TOK_LAYOUT layout_mode
    {
        json_output = cmd_layout(&current_match,
                   ($2 == L_DEFAULT ? "default" :
                    ($2 == L_STACKED ? "stacked" :
                     "tabbed")));
    }
    ;

layout_mode:
    TOK_DEFAULT   { $$ = L_DEFAULT; }
    | TOK_STACKED { $$ = L_STACKED; }
    | TOK_TABBED  { $$ = L_TABBED; }
    ;

mark:
    TOK_MARK STR
    {
        json_output = cmd_mark(&current_match, $2);
        free($2);
    }
    ;

nop:
    TOK_NOP STR
    {
        json_output = cmd_nop(&current_match, $2);
        free($2);
    }
    ;

scratchpad:
    TOK_SCRATCHPAD TOK_SHOW
    {
        json_output = cmd_scratchpad_show(&current_match);
    }
    ;


resize:
    TOK_RESIZE resize_way direction resize_px resize_tiling
    {
        char buffer1[128], buffer2[128];
        snprintf(buffer1, sizeof(buffer1), "%d", $4);
        snprintf(buffer2, sizeof(buffer2), "%d", $5);
        json_output = cmd_resize(&current_match,
                   ($2 == TOK_SHRINK ? "shrink" : "grow"),
                   ($3 == TOK_LEFT ? "left" :
                    ($3 == TOK_RIGHT ? "right" :
                     ($3 == TOK_DOWN ? "down" :
                      "up"))),
                   buffer1,
                   buffer2);
    }
    ;

resize_px:
    /* empty */
    {
        $$ = 10;
    }
    | NUMBER TOK_PX
    {
        $$ = $1;
    }
    ;

resize_tiling:
    /* empty */
    {
        $$ = 10;
    }
    | TOK_OR NUMBER TOK_PPT
    {
        $$ = $2;
    }
    ;

resize_way:
    TOK_GROW        { $$ = TOK_GROW; }
    | TOK_SHRINK    { $$ = TOK_SHRINK; }
    ;

direction:
    TOK_UP          { $$ = TOK_UP; }
    | TOK_DOWN      { $$ = TOK_DOWN; }
    | TOK_LEFT      { $$ = TOK_LEFT; }
    | TOK_RIGHT     { $$ = TOK_RIGHT; }
    ;

mode:
    TOK_MODE STR
    {
        json_output = cmd_mode(&current_match, $2);
        free($2);
    }
    ;
