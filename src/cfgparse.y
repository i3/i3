%{
/*
 * vim:ts=4:sw=4:expandtab
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "all.h"

typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern int yylex(struct context *context);
extern int yyparse(void);
extern FILE *yyin;
YY_BUFFER_STATE yy_scan_string(const char *);

static struct bindings_head *current_bindings;
static struct context *context;

/* We don’t need yydebug for now, as we got decent error messages using
 * yyerror(). Should you ever want to extend the parser, it might be handy
 * to just comment it in again, so it stays here. */
//int yydebug = 1;

void yyerror(const char *error_message) {
    ELOG("\n");
    ELOG("CONFIG: %s\n", error_message);
    ELOG("CONFIG: in file \"%s\", line %d:\n",
        context->filename, context->line_number);
    ELOG("CONFIG:   %s\n", context->line_copy);
    ELOG("CONFIG:   ");
    for (int c = 1; c <= context->last_column; c++)
        if (c >= context->first_column)
            printf("^");
        else printf(" ");
    printf("\n");
    ELOG("\n");
}

int yywrap() {
    return 1;
}

void parse_file(const char *f) {
    SLIST_HEAD(variables_head, Variable) variables = SLIST_HEAD_INITIALIZER(&variables);
    int fd, ret, read_bytes = 0;
    struct stat stbuf;
    char *buf;
    FILE *fstr;
    char buffer[1026], key[512], value[512];

    if ((fd = open(f, O_RDONLY)) == -1)
        die("Could not open configuration file: %s\n", strerror(errno));

    if (fstat(fd, &stbuf) == -1)
        die("Could not fstat file: %s\n", strerror(errno));

    buf = scalloc((stbuf.st_size + 1) * sizeof(char));
    while (read_bytes < stbuf.st_size) {
        if ((ret = read(fd, buf + read_bytes, (stbuf.st_size - read_bytes))) < 0)
            die("Could not read(): %s\n", strerror(errno));
        read_bytes += ret;
    }

    if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
        die("Could not lseek: %s\n", strerror(errno));

    if ((fstr = fdopen(fd, "r")) == NULL)
        die("Could not fdopen: %s\n", strerror(errno));

    while (!feof(fstr)) {
        if (fgets(buffer, 1024, fstr) == NULL) {
            if (feof(fstr))
                break;
            die("Could not read configuration file\n");
        }

        /* sscanf implicitly strips whitespace. Also, we skip comments and empty lines. */
        if (sscanf(buffer, "%s %[^\n]", key, value) < 1 ||
            key[0] == '#' || strlen(key) < 3)
            continue;

        if (strcasecmp(key, "set") == 0) {
            if (value[0] != '$')
                die("Malformed variable assignment, name has to start with $\n");

            /* get key/value for this variable */
            char *v_key = value, *v_value;
            if ((v_value = strstr(value, " ")) == NULL)
                die("Malformed variable assignment, need a value\n");

            *(v_value++) = '\0';

            struct Variable *new = scalloc(sizeof(struct Variable));
            new->key = sstrdup(v_key);
            new->value = sstrdup(v_value);
            SLIST_INSERT_HEAD(&variables, new, variables);
            DLOG("Got new variable %s = %s\n", v_key, v_value);
            continue;
        }
    }
    fclose(fstr);

    /* For every custom variable, see how often it occurs in the file and
     * how much extra bytes it requires when replaced. */
    struct Variable *current, *nearest;
    int extra_bytes = 0;
    /* We need to copy the buffer because we need to invalidate the
     * variables (otherwise we will count them twice, which is bad when
     * 'extra' is negative) */
    char *bufcopy = sstrdup(buf);
    SLIST_FOREACH(current, &variables, variables) {
        int extra = (strlen(current->value) - strlen(current->key));
        char *next;
        for (next = bufcopy;
             (next = strcasestr(bufcopy + (next - bufcopy), current->key)) != NULL;
             next += strlen(current->key)) {
            *next = '_';
            extra_bytes += extra;
        }
    }
    FREE(bufcopy);

    /* Then, allocate a new buffer and copy the file over to the new one,
     * but replace occurences of our variables */
    char *walk = buf, *destwalk;
    char *new = smalloc((stbuf.st_size + extra_bytes + 1) * sizeof(char));
    destwalk = new;
    while (walk < (buf + stbuf.st_size)) {
        /* Find the next variable */
        SLIST_FOREACH(current, &variables, variables)
            current->next_match = strcasestr(walk, current->key);
        nearest = NULL;
        int distance = stbuf.st_size;
        SLIST_FOREACH(current, &variables, variables) {
            if (current->next_match == NULL)
                continue;
            if ((current->next_match - walk) < distance) {
                distance = (current->next_match - walk);
                nearest = current;
            }
        }
        if (nearest == NULL) {
            /* If there are no more variables, we just copy the rest */
            strncpy(destwalk, walk, (buf + stbuf.st_size) - walk);
            destwalk += (buf + stbuf.st_size) - walk;
            *destwalk = '\0';
            break;
        } else {
            /* Copy until the next variable, then copy its value */
            strncpy(destwalk, walk, distance);
            strncpy(destwalk + distance, nearest->value, strlen(nearest->value));
            walk += distance + strlen(nearest->key);
            destwalk += distance + strlen(nearest->value);
        }
    }

    yy_scan_string(new);

    context = scalloc(sizeof(struct context));
    context->filename = f;

    if (yyparse() != 0) {
        fprintf(stderr, "Could not parse configfile\n");
        exit(1);
    }

    FREE(context->line_copy);
    free(context);
    free(new);
    free(buf);

    while (!SLIST_EMPTY(&variables)) {
        current = SLIST_FIRST(&variables);
        FREE(current->key);
        FREE(current->value);
        SLIST_REMOVE_HEAD(&variables, variables);
        FREE(current);
    }
}

%}

%expect 1
%error-verbose
%lex-param { struct context *context }

%union {
    int number;
    char *string;
    uint32_t *single_color;
    struct Colortriple *color;
    Match *match;
    struct Binding *binding;
}

%token  <number>        NUMBER                      "<number>"
%token  <string>        WORD                        "<word>"
%token  <string>        STR                         "<string>"
%token  <string>        STR_NG                      "<string (non-greedy)>"
%token  <string>        HEX                         "<hex>"
%token  <string>        OUTPUT                      "<RandR output>"
%token                  TOKBINDCODE
%token                  TOKTERMINAL
%token                  TOKCOMMENT                  "<comment>"
%token                  TOKFONT                     "font"
%token                  TOKBINDSYM                  "bindsym"
%token  <number>        MODIFIER                    "<modifier>"
%token                  TOKCONTROL                  "control"
%token                  TOKSHIFT                    "shift"
%token                  WHITESPACE                  "<whitespace>"
%token                  TOKFLOATING_MODIFIER        "floating_modifier"
%token  <string>        QUOTEDSTRING                "<quoted string>"
%token                  TOKWORKSPACE                "workspace"
%token                  TOKOUTPUT                   "output"
%token                  TOKASSIGN                   "assign"
%token                  TOKSET
%token                  TOKIPCSOCKET                "ipc_socket"
%token                  TOKRESTARTSTATE             "restart_state"
%token                  TOKEXEC                     "exec"
%token  <single_color>  TOKSINGLECOLOR
%token  <color>         TOKCOLOR
%token                  TOKARROW                    "→"
%token                  TOKMODE                     "mode"
%token                  TOK_ORIENTATION             "default_orientation"
%token                  TOK_HORIZ                   "horizontal"
%token                  TOK_VERT                    "vertical"
%token                  TOK_AUTO                    "auto"
%token                  TOK_WORKSPACE_LAYOUT        "workspace_layout"
%token                  TOKNEWWINDOW                "new_window"
%token                  TOK_NORMAL                  "normal"
%token                  TOK_NONE                    "none"
%token                  TOK_1PIXEL                  "1pixel"
%token                  TOKFOCUSFOLLOWSMOUSE        "focus_follows_mouse"
%token                  TOKWORKSPACEBAR             "workspace_bar"
%token                  TOK_DEFAULT                 "default"
%token                  TOK_STACKING                "stacking"
%token                  TOK_TABBED                  "tabbed"
%token  <number>        TOKSTACKLIMIT               "stack-limit"
%token                  TOK_POPUP_DURING_FULLSCREEN "popup_during_fullscreen"
%token                  TOK_IGNORE                  "ignore"
%token                  TOK_LEAVE_FULLSCREEN        "leave_fullscreen"

%type   <binding>       binding
%type   <binding>       bindcode
%type   <binding>       bindsym
%type   <number>        binding_modifiers
%type   <number>        binding_modifier
%type   <number>        direction
%type   <number>        layout_mode
%type   <number>        border_style
%type   <number>        new_window
%type   <number>        colorpixel
%type   <number>        bool
%type   <number>        popup_setting
%type   <string>        command
%type   <string>        word_or_number
%type   <string>        optional_workspace_name
%type   <string>        workspace_name
%type   <string>        window_class
%type   <match>         assign_target

%%

lines: /* empty */
    | lines WHITESPACE line
    | lines error
    | lines line
    ;

line:
    bindline
    | mode
    | floating_modifier
    | orientation
    | workspace_layout
    | new_window
    | focus_follows_mouse
    | workspace_bar
    | workspace
    | assign
    | ipcsocket
    | restart_state
    | exec
    | single_color
    | color
    | terminal
    | font
    | comment
    | popup_during_fullscreen
    ;

comment:
    TOKCOMMENT
    ;

command:
    STR
    ;

bindline:
    binding
    {
        TAILQ_INSERT_TAIL(bindings, $1, bindings);
    }
    ;

binding:
    TOKBINDCODE WHITESPACE bindcode         { $$ = $3; }
    | TOKBINDSYM WHITESPACE bindsym         { $$ = $3; }
    ;

bindcode:
    binding_modifiers NUMBER WHITESPACE command
    {
        printf("\tFound keycode binding mod%d with key %d and command %s\n", $1, $2, $4);
        Binding *new = scalloc(sizeof(Binding));

        new->keycode = $2;
        new->mods = $1;
        new->command = $4;

        $$ = new;
    }
    ;

bindsym:
    binding_modifiers word_or_number WHITESPACE command
    {
        printf("\tFound keysym binding mod%d with key %s and command %s\n", $1, $2, $4);
        Binding *new = scalloc(sizeof(Binding));

        new->symbol = $2;
        new->mods = $1;
        new->command = $4;

        $$ = new;
    }
    ;

word_or_number:
    WORD
    | NUMBER
    {
        asprintf(&$$, "%d", $1);
    }
    ;

mode:
    TOKMODE WHITESPACE QUOTEDSTRING WHITESPACE '{' modelines '}'
    {
        if (strcasecmp($3, "default") == 0) {
            printf("You cannot use the name \"default\" for your mode\n");
            exit(1);
        }
        printf("\t now in mode %s\n", $3);
        printf("\t current bindings = %p\n", current_bindings);
        Binding *binding;
        TAILQ_FOREACH(binding, current_bindings, bindings) {
            printf("got binding on mods %d, keycode %d, symbol %s, command %s\n",
                            binding->mods, binding->keycode, binding->symbol, binding->command);
        }

        struct Mode *mode = scalloc(sizeof(struct Mode));
        mode->name = $3;
        mode->bindings = current_bindings;
        current_bindings = NULL;
        SLIST_INSERT_HEAD(&modes, mode, modes);
    }
    ;


modelines:
    /* empty */
    | modelines modeline
    ;

modeline:
    WHITESPACE
    | comment
    | binding
    {
        if (current_bindings == NULL) {
            current_bindings = scalloc(sizeof(struct bindings_head));
            TAILQ_INIT(current_bindings);
        }

        TAILQ_INSERT_TAIL(current_bindings, $1, bindings);
    }
    ;

floating_modifier:
    TOKFLOATING_MODIFIER WHITESPACE binding_modifiers
    {
        DLOG("floating modifier = %d\n", $3);
        config.floating_modifier = $3;
    }
    ;

orientation:
    TOK_ORIENTATION WHITESPACE direction
    {
        DLOG("New containers should start with split direction %d\n", $3);
        config.default_orientation = $3;
    }
    ;

direction:
    TOK_HORIZ       { $$ = HORIZ; }
    | TOK_VERT      { $$ = VERT; }
    | TOK_AUTO      { $$ = NO_ORIENTATION; }
    ;

workspace_layout:
    TOK_WORKSPACE_LAYOUT WHITESPACE layout_mode
    {
        DLOG("new containers will be in mode %d\n", $3);
        config.default_layout = $3;

#if 0
        /* We also need to change the layout of the already existing
         * workspaces here. Workspaces may exist at this point because
         * of the other directives which are modifying workspaces
         * (setting the preferred screen or name). While the workspace
         * objects are already created, they have never been used.
         * Thus, the user very likely awaits the default container mode
         * to trigger in this case, regardless of where it is inside
         * his configuration file. */
        Workspace *ws;
        TAILQ_FOREACH(ws, workspaces, workspaces) {
                if (ws->table == NULL)
                        continue;
                switch_layout_mode(global_conn,
                                   ws->table[0][0],
                                   config.container_mode);
        }
#endif
    }
    | TOK_WORKSPACE_LAYOUT WHITESPACE TOKSTACKLIMIT WHITESPACE TOKSTACKLIMIT WHITESPACE NUMBER
    {
        DLOG("stack-limit %d with val %d\n", $5, $7);
        config.container_stack_limit = $5;
        config.container_stack_limit_value = $7;

#if 0
        /* See the comment above */
        Workspace *ws;
        TAILQ_FOREACH(ws, workspaces, workspaces) {
                if (ws->table == NULL)
                        continue;
                Container *con = ws->table[0][0];
                con->stack_limit = config.container_stack_limit;
                con->stack_limit_value = config.container_stack_limit_value;
        }
#endif
    }
    ;

layout_mode:
    TOK_DEFAULT       { $$ = L_DEFAULT; }
    | TOK_STACKING    { $$ = L_STACKED; }
    | TOK_TABBED      { $$ = L_TABBED; }
    ;

new_window:
    TOKNEWWINDOW WHITESPACE border_style
    {
        DLOG("new windows should start with border style %d\n", $3);
        config.default_border = $3;
    }
    ;

border_style:
    TOK_NORMAL      { $$ = BS_NORMAL; }
    | TOK_NONE      { $$ = BS_NONE; }
    | TOK_1PIXEL    { $$ = BS_1PIXEL; }
    ;

bool:
    NUMBER
    {
        $$ = ($1 == 1);
    }
    | WORD
    {
        DLOG("checking word \"%s\"\n", $1);
        $$ = (strcasecmp($1, "yes") == 0 ||
              strcasecmp($1, "true") == 0 ||
              strcasecmp($1, "on") == 0 ||
              strcasecmp($1, "enable") == 0 ||
              strcasecmp($1, "active") == 0);
    }
    ;

focus_follows_mouse:
    TOKFOCUSFOLLOWSMOUSE WHITESPACE bool
    {
        DLOG("focus follows mouse = %d\n", $3);
        config.disable_focus_follows_mouse = !($3);
    }
    ;

workspace_bar:
    TOKWORKSPACEBAR WHITESPACE bool
    {
        DLOG("workspace bar = %d\n", $3);
        config.disable_workspace_bar = !($3);
    }
    ;

workspace:
    TOKWORKSPACE WHITESPACE NUMBER WHITESPACE TOKOUTPUT WHITESPACE OUTPUT optional_workspace_name
    {
        int ws_num = $3;
        if (ws_num < 1) {
            DLOG("Invalid workspace assignment, workspace number %d out of range\n", ws_num);
        } else {
            char *ws_name = NULL;
            if ($8 == NULL) {
                asprintf(&ws_name, "%d", ws_num);
            } else {
                ws_name = $8;
            }

            DLOG("Should assign workspace %s to output %s\n", ws_name, $7);
            struct Workspace_Assignment *assignment = scalloc(sizeof(struct Workspace_Assignment));
            assignment->name = ws_name;
            assignment->output = $7;
            TAILQ_INSERT_TAIL(&ws_assignments, assignment, ws_assignments);
        }
    }
    | TOKWORKSPACE WHITESPACE NUMBER WHITESPACE workspace_name
    {
        int ws_num = $3;
        if (ws_num < 1) {
            DLOG("Invalid workspace assignment, workspace number %d out of range\n", ws_num);
        } else {
            DLOG("workspace name to: %s\n", $5);
#if 0
            if ($<string>5 != NULL) {
                    workspace_set_name(workspace_get(ws_num - 1), $<string>5);
                    free($<string>5);
            }
#endif
        }
    }
    ;

optional_workspace_name:
    /* empty */                     { $$ = NULL; }
    | WHITESPACE workspace_name     { $$ = $2; }
    ;

workspace_name:
    QUOTEDSTRING         { $$ = $1; }
    | STR                { $$ = $1; }
    | WORD               { $$ = $1; }
    ;

assign:
    TOKASSIGN WHITESPACE window_class WHITESPACE optional_arrow assign_target
    {
        printf("assignment of %s\n", $3);

        struct Match *match = $6;

        char *separator = NULL;
        if ((separator = strchr($3, '/')) != NULL) {
            *(separator++) = '\0';
            match->title = sstrdup(separator);
        }
        if (*$3 != '\0')
            match->class = sstrdup($3);
        free($3);

        printf("  class = %s\n", match->class);
        printf("  title = %s\n", match->title);
        if (match->insert_where == M_ASSIGN_WS)
            printf("  to ws %s\n", match->target_ws);
        TAILQ_INSERT_TAIL(&assignments, match, assignments);
    }
    ;

assign_target:
    NUMBER
    {
        /* TODO: named workspaces */
        Match *match = smalloc(sizeof(Match));
        match_init(match);
        match->insert_where = M_ASSIGN_WS;
        asprintf(&(match->target_ws), "%d", $1);
        $$ = match;
    }
    | '~'
    {
        /* TODO: compatiblity */
#if 0
        struct Assignment *new = scalloc(sizeof(struct Assignment));
        new->floating = ASSIGN_FLOATING_ONLY;
        $<assignment>$ = new;
#endif
    }
    | '~' NUMBER
    {
        /* TODO: compatiblity */
#if 0
        struct Assignment *new = scalloc(sizeof(struct Assignment));
        new->workspace = $<number>2;
        new->floating = ASSIGN_FLOATING;
        $<assignment>$ = new;
#endif
    }
    ;

window_class:
    QUOTEDSTRING
    | STR_NG
    ;

optional_arrow:
    /* NULL */
    | TOKARROW WHITESPACE
    ;

ipcsocket:
    TOKIPCSOCKET WHITESPACE STR
    {
        config.ipc_socket_path = $3;
    }
    ;

restart_state:
    TOKRESTARTSTATE WHITESPACE STR
    {
        config.restart_state_path = $3;
    }
    ;

exec:
    TOKEXEC WHITESPACE STR
    {
        struct Autostart *new = smalloc(sizeof(struct Autostart));
        new->command = $3;
        TAILQ_INSERT_TAIL(&autostarts, new, autostarts);
    }
    ;

terminal:
    TOKTERMINAL WHITESPACE STR
    {
        ELOG("The terminal option is DEPRECATED and has no effect. "
            "Please remove it from your configuration file.\n");
    }
    ;

font:
    TOKFONT WHITESPACE STR
    {
        config.font = load_font($3, true);
        printf("font %s\n", $3);
    }
    ;

single_color:
    TOKSINGLECOLOR WHITESPACE colorpixel
    {
        uint32_t *dest = $1;
        *dest = $3;
    }
    ;

color:
    TOKCOLOR WHITESPACE colorpixel WHITESPACE colorpixel WHITESPACE colorpixel
    {
        struct Colortriple *dest = $1;

        dest->border = $3;
        dest->background = $5;
        dest->text = $7;
    }
    ;

colorpixel:
    '#' HEX
    {
        char *hex;
        if (asprintf(&hex, "#%s", $2) == -1)
            die("asprintf()");
        $$ = get_colorpixel(hex);
        free(hex);
    }
    ;


binding_modifiers:
    /* NULL */                               { $$ = 0; }
    | binding_modifier
    | binding_modifiers '+' binding_modifier { $$ = $1 | $3; }
    | binding_modifiers '+'                  { $$ = $1; }
    ;

binding_modifier:
    MODIFIER        { $$ = $1; }
    | TOKCONTROL    { $$ = BIND_CONTROL; }
    | TOKSHIFT      { $$ = BIND_SHIFT; }
    ;

popup_during_fullscreen:
    TOK_POPUP_DURING_FULLSCREEN WHITESPACE popup_setting
    {
        DLOG("popup_during_fullscreen setting: %d\n", $3);
        config.popup_during_fullscreen = $3;
    }
    ;

popup_setting:
    TOK_IGNORE              { $$ = PDF_IGNORE; }
    | TOK_LEAVE_FULLSCREEN  { $$ = PDF_LEAVE_FULLSCREEN; }
    ;
