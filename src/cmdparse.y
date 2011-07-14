%{
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2011 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * cmdparse.y: the parser for commands you send to i3 (or bind on keys)
 *

 */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "all.h"

/** When the command did not include match criteria (!), we use the currently
 * focused command. Do not confuse this case with a command which included
 * criteria but which did not match any windows. This macro has to be called in
 * every command.
 */
#define HANDLE_EMPTY_MATCH do { \
    if (match_is_empty(&current_match)) { \
        owindow *ow = smalloc(sizeof(owindow)); \
        ow->con = focused; \
        TAILQ_INIT(&owindows); \
        TAILQ_INSERT_TAIL(&owindows, ow, owindows); \
    } \
} while (0)

typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern int cmdyylex(struct context *context);
extern int cmdyyparse(void);
extern FILE *cmdyyin;
YY_BUFFER_STATE cmdyy_scan_string(const char *);

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
    LOG("COMMAND: *%s*\n", new);
    cmdyy_scan_string(new);

    match_init(&current_match);
    context = scalloc(sizeof(struct context));
    context->filename = "cmd";
    FREE(json_output);
    if (cmdyyparse() != 0) {
        fprintf(stderr, "Could not parse command\n");
        asprintf(&json_output, "{\"success\":false, \"error\":\"%s at position %d\"}",
                 context->compact_error, context->first_column);
        FREE(context->line_copy);
        FREE(context->compact_error);
        free(context);
        return json_output;
    }
    printf("done, json output = %s\n", json_output);

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
%token              TOK_TOGGLE          "toggle"
%token              TOK_FOCUS           "focus"
%token              TOK_MOVE            "move"
%token              TOK_OPEN            "open"
%token              TOK_NEXT            "next"
%token              TOK_PREV            "prev"
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

%token              TOK_CLASS           "class"
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

%%

commands:
    commands ';' command
    | command
    {
        owindow *current;

        printf("single command completely parsed, dropping state...\n");
        while (!TAILQ_EMPTY(&owindows)) {
            current = TAILQ_FIRST(&owindows);
            TAILQ_REMOVE(&owindows, current, owindows);
            free(current);
        }
        match_init(&current_match);
    }
    ;

command:
    match operations
    ;

match:
    | matchstart criteria matchend
    {
        printf("match parsed\n");
    }
    ;

matchstart:
    '['
    {
        printf("start\n");
        match_init(&current_match);
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
    criteria criterion
    | criterion
    ;

criterion:
    TOK_CLASS '=' STR
    {
        printf("criteria: class = %s\n", $3);
        current_match.class = $3;
    }
    | TOK_CON_ID '=' STR
    {
        printf("criteria: id = %s\n", $3);
        char *end;
        long parsed = strtol($3, &end, 10);
        if (parsed == LONG_MIN ||
            parsed == LONG_MAX ||
            parsed < 0 ||
            (end && *end != '\0')) {
            ELOG("Could not parse con id \"%s\"\n", $3);
        } else {
            current_match.con_id = (Con*)parsed;
            printf("id as int = %p\n", current_match.con_id);
        }
    }
    | TOK_ID '=' STR
    {
        printf("criteria: window id = %s\n", $3);
        char *end;
        long parsed = strtol($3, &end, 10);
        if (parsed == LONG_MIN ||
            parsed == LONG_MAX ||
            parsed < 0 ||
            (end && *end != '\0')) {
            ELOG("Could not parse window id \"%s\"\n", $3);
        } else {
            current_match.id = parsed;
            printf("window id as int = %d\n", current_match.id);
        }
    }
    | TOK_MARK '=' STR
    {
        printf("criteria: mark = %s\n", $3);
        current_match.mark = $3;
    }
    | TOK_TITLE '=' STR
    {
        printf("criteria: title = %s\n", $3);
        current_match.title = $3;
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
    | mode
    ;

exec:
    TOK_EXEC STR
    {
        printf("should execute %s\n", $2);
        start_application($2);
        free($2);
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
        kill_configerror_nagbar(false);
        load_configuration(conn, NULL, true);
        x_set_i3_atoms();
        /* Send an IPC event just in case the ws names have changed */
        ipc_send_event("workspace", I3_IPC_EVENT_WORKSPACE, "{\"change\":\"reload\"}");
    }
    ;

restart:
    TOK_RESTART
    {
        printf("restarting i3\n");
        i3_restart(false);
    }
    ;

focus:
    TOK_FOCUS
    {
        owindow *current;

        if (match_is_empty(&current_match)) {
            ELOG("You have to specify which window/container should be focused.\n");
            ELOG("Example: [class=\"urxvt\" title=\"irssi\"] focus\n");

            asprintf(&json_output, "{\"success\":false, \"error\":\"You have to "
                     "specify which window/container should be focused\"}");
            break;
        }

        int count = 0;
        TAILQ_FOREACH(current, &owindows, owindows) {
            Con *ws = con_get_workspace(current->con);
            workspace_show(ws->name);
            LOG("focusing %p / %s\n", current->con, current->con->name);
            con_focus(current->con);
            count++;
        }

        if (count > 1)
            LOG("WARNING: Your criteria for the focus command matches %d containers, "
                "while only exactly one container can be focused at a time.\n", count);

        tree_render();
    }
    | TOK_FOCUS direction
    {
        int direction = $2;
        switch (direction) {
            case TOK_LEFT:
                LOG("Focusing left\n");
                tree_next('p', HORIZ);
                break;
            case TOK_RIGHT:
                LOG("Focusing right\n");
                tree_next('n', HORIZ);
                break;
            case TOK_UP:
                LOG("Focusing up\n");
                tree_next('p', VERT);
                break;
            case TOK_DOWN:
                LOG("Focusing down\n");
                tree_next('n', VERT);
                break;
            default:
                ELOG("Invalid focus direction (%d)\n", direction);
                break;
        }

        tree_render();
    }
    | TOK_FOCUS window_mode
    {
        printf("should focus: ");

        if ($2 == TOK_TILING)
            printf("tiling\n");
        else if ($2 == TOK_FLOATING)
            printf("floating\n");
        else printf("mode toggle\n");

        Con *ws = con_get_workspace(focused);
        Con *current;
        if (ws != NULL) {
            int to_focus = $2;
            if ($2 == TOK_MODE_TOGGLE) {
                current = TAILQ_FIRST(&(ws->focus_head));
                if (current->type == CT_FLOATING_CON)
                    to_focus = TOK_TILING;
                else to_focus = TOK_FLOATING;
            }
            TAILQ_FOREACH(current, &(ws->focus_head), focused) {
                if ((to_focus == TOK_FLOATING && current->type != CT_FLOATING_CON) ||
                    (to_focus == TOK_TILING && current->type == CT_FLOATING_CON))
                    continue;

                con_focus(con_descend_focused(current));
                break;
            }
        }

        tree_render();
    }
    | TOK_FOCUS level
    {
        if ($2 == TOK_PARENT)
            level_up();
        else level_down();

        tree_render();
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
        owindow *current;

        printf("killing!\n");
        /* check if the match is empty, not if the result is empty */
        if (match_is_empty(&current_match))
            tree_close_con($2);
        else {
            TAILQ_FOREACH(current, &owindows, owindows) {
                printf("matching: %p / %s\n", current->con, current->con->name);
                tree_close(current->con, $2, false);
            }
        }

        tree_render();
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
        workspace_next();
        tree_render();
    }
    | TOK_WORKSPACE TOK_PREV
    {
        workspace_prev();
        tree_render();
    }
    | TOK_WORKSPACE STR
    {
        printf("should switch to workspace %s\n", $2);
        workspace_show($2);
        free($2);

        tree_render();
    }
    ;

open:
    TOK_OPEN
    {
        printf("opening new container\n");
        Con *con = tree_open_con(NULL, NULL);
        con_focus(con);
        asprintf(&json_output, "{\"success\":true, \"id\":%ld}", (long int)con);

        tree_render();
    }
    ;

fullscreen:
    TOK_FULLSCREEN fullscreen_mode
    {
        printf("toggling fullscreen, mode = %s\n", ($2 == CF_OUTPUT ? "normal" : "global"));
        owindow *current;

        HANDLE_EMPTY_MATCH;

        TAILQ_FOREACH(current, &owindows, owindows) {
            printf("matching: %p / %s\n", current->con, current->con->name);
            con_toggle_fullscreen(current->con, $2);
        }

        tree_render();
    }
    ;

fullscreen_mode:
    /* empty */  { $$ = CF_OUTPUT; }
    | TOK_GLOBAL { $$ = CF_GLOBAL; }
    ;

split:
    TOK_SPLIT split_direction
    {
        /* TODO: use matches */
        printf("splitting in direction %c\n", $2);
        tree_split(focused, ($2 == 'v' ? VERT : HORIZ));

        tree_render();
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
        HANDLE_EMPTY_MATCH;

        owindow *current;
        TAILQ_FOREACH(current, &owindows, owindows) {
            printf("matching: %p / %s\n", current->con, current->con->name);
            if ($2 == TOK_TOGGLE) {
                printf("should toggle mode\n");
                toggle_floating_mode(current->con, false);
            } else {
                printf("should switch mode to %s\n", ($2 == TOK_FLOATING ? "floating" : "tiling"));
                if ($2 == TOK_ENABLE) {
                    floating_enable(current->con, false);
                } else {
                    floating_disable(current->con, false);
                }
            }
        }

        tree_render();
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
        printf("border style should be changed to %d\n", $2);
        owindow *current;

        HANDLE_EMPTY_MATCH;

        TAILQ_FOREACH(current, &owindows, owindows) {
            printf("matching: %p / %s\n", current->con, current->con->name);
            if ($2 == TOK_TOGGLE) {
                current->con->border_style++;
                current->con->border_style %= 3;
            } else current->con->border_style = $2;
        }

        tree_render();
    }
    ;

border_style:
    TOK_NORMAL      { $$ = BS_NORMAL; }
    | TOK_NONE      { $$ = BS_NONE; }
    | TOK_1PIXEL    { $$ = BS_1PIXEL; }
    | TOK_TOGGLE    { $$ = TOK_TOGGLE; }
    ;

move:
    TOK_MOVE direction
    {
        printf("moving in direction %d\n", $2);
        tree_move($2);

        tree_render();
    }
    | TOK_MOVE TOK_WORKSPACE STR
    {
        owindow *current;

        printf("should move window to workspace %s\n", $3);
        /* get the workspace */
        Con *ws = workspace_get($3, NULL);
        free($3);

        HANDLE_EMPTY_MATCH;

        TAILQ_FOREACH(current, &owindows, owindows) {
            printf("matching: %p / %s\n", current->con, current->con->name);
            con_move_to_workspace(current->con, ws);
        }

        tree_render();
    }
    ;

append_layout:
    TOK_APPEND_LAYOUT STR
    {
        printf("restoring \"%s\"\n", $2);
        tree_append_json($2);
        free($2);
        tree_render();
    }
    ;

layout:
    TOK_LAYOUT layout_mode
    {
        printf("changing layout to %d\n", $2);
        owindow *current;

        /* check if the match is empty, not if the result is empty */
        if (match_is_empty(&current_match))
            con_set_layout(focused->parent, $2);
        else {
            TAILQ_FOREACH(current, &owindows, owindows) {
                printf("matching: %p / %s\n", current->con, current->con->name);
                con_set_layout(current->con, $2);
            }
        }

        tree_render();
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
        printf("marking window with str %s\n", $2);
        owindow *current;

        HANDLE_EMPTY_MATCH;

        TAILQ_FOREACH(current, &owindows, owindows) {
            printf("matching: %p / %s\n", current->con, current->con->name);
            current->con->mark = sstrdup($2);
        }

        free($<string>2);

        tree_render();
    }
    ;

nop:
    TOK_NOP STR
    {
        printf("-------------------------------------------------\n");
        printf("  NOP: %s\n", $2);
        printf("-------------------------------------------------\n");
        free($2);
    }
    ;

resize:
    TOK_RESIZE resize_way direction resize_px resize_tiling
    {
        /* resize <grow|shrink> <direction> [<px> px] [or <ppt> ppt] */
        printf("resizing in way %d, direction %d, px %d or ppt %d\n", $2, $3, $4, $5);
        int direction = $3;
        int px = $4;
        int ppt = $5;
        if ($2 == TOK_SHRINK) {
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
            int children = con_num_children(focused->parent);
            Con *other;
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

        tree_render();
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
        switch_mode($2);
    }
    ;
