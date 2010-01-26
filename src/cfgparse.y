%{
/*
 * vim:ts=8:expandtab
 *
 */
#include <stdio.h>
#include <string.h>
#include <xcb/xcb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#include "data.h"
#include "config.h"
#include "i3.h"
#include "util.h"
#include "queue.h"
#include "table.h"
#include "workspace.h"
#include "xcb.h"
#include "log.h"

typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern int yylex(void);
extern int yyparse(void);
extern FILE *yyin;
YY_BUFFER_STATE yy_scan_string(const char *);

static struct bindings_head *current_bindings;

int yydebug = 1;

void yyerror(const char *str) {
        fprintf(stderr,"error: %s\n",str);
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

        buf = smalloc(stbuf.st_size * sizeof(char));
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

        /* For every custom variable, see how often it occurs in the file and
         * how much extra bytes it requires when replaced. */
        struct Variable *current, *nearest;
        int extra_bytes = 0;
        SLIST_FOREACH(current, &variables, variables) {
                int extra = (strlen(current->value) - strlen(current->key));
                char *next;
                for (next = buf;
                     (next = strcasestr(buf + (next - buf), current->key)) != NULL;
                     next += strlen(current->key))
                        extra_bytes += extra;
        }

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

        if (yyparse() != 0) {
                fprintf(stderr, "Could not parse configfile\n");
                exit(1);
        }

        free(new);
        free(buf);
}

%}

%expect 1

%union {
        int number;
        char *string;
        struct Colortriple *color;
        struct Assignment *assignment;
        struct Binding *binding;
}

%token <number>NUMBER
%token <string>WORD
%token <string>STR
%token <string>STR_NG
%token <string>HEX
%token TOKBIND
%token TOKTERMINAL
%token TOKCOMMENT
%token TOKFONT
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
%token TOKMODE
%token TOKNEWCONTAINER
%token TOKNEWWINDOW
%token TOKFOCUSFOLLOWSMOUSE
%token TOKCONTAINERMODE
%token TOKSTACKLIMIT

%%

lines: /* empty */
        | lines WHITESPACE line
        | lines line
        ;

line:
        bindline
        | mode
        | floating_modifier
        | new_container
        | new_window
        | focus_follows_mouse
        | workspace
        | assign
        | ipcsocket
        | exec
        | color
        | terminal
        | font
        | comment
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
                TAILQ_INSERT_TAIL(bindings, $<binding>1, bindings);
        }
        ;

binding:
        TOKBIND WHITESPACE bind                 { $<binding>$ = $<binding>3; }
        | TOKBINDSYM WHITESPACE bindsym         { $<binding>$ = $<binding>3; }
        ;

bind:
        binding_modifiers NUMBER WHITESPACE command
        {
                printf("\tFound binding mod%d with key %d and command %s\n", $<number>1, $2, $<string>4);
                Binding *new = scalloc(sizeof(Binding));

                new->keycode = $<number>2;
                new->mods = $<number>1;
                new->command = sstrdup($<string>4);

                $<binding>$ = new;
        }
        ;

bindsym:
        binding_modifiers word_or_number WHITESPACE command
        {
                printf("\tFound symbolic mod%d with key %s and command %s\n", $<number>1, $<string>2, $<string>4);
                Binding *new = scalloc(sizeof(Binding));

                new->symbol = sstrdup($<string>2);
                new->mods = $<number>1;
                new->command = sstrdup($<string>4);

                $<binding>$ = new;
        }
        ;

word_or_number:
        WORD
        | NUMBER
        {
                asprintf(&$<string>$, "%d", $1);
        }
        ;

mode:
        TOKMODE WHITESPACE QUOTEDSTRING WHITESPACE '{' modelines '}'
        {
                if (strcasecmp($<string>3, "default") == 0) {
                        printf("You cannot use the name \"default\" for your mode\n");
                        exit(1);
                }
                printf("\t now in mode %s\n", $<string>3);
                printf("\t current bindings = %p\n", current_bindings);
                Binding *binding;
                TAILQ_FOREACH(binding, current_bindings, bindings) {
                        printf("got binding on mods %d, keycode %d, symbol %s, command %s\n",
                                        binding->mods, binding->keycode, binding->symbol, binding->command);
                }

                struct Mode *mode = scalloc(sizeof(struct Mode));
                mode->name = strdup($<string>3);
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

                TAILQ_INSERT_TAIL(current_bindings, $<binding>1, bindings);
        }
        ;

floating_modifier:
        TOKFLOATING_MODIFIER WHITESPACE binding_modifiers
        {
                DLOG("floating modifier = %d\n", $<number>3);
                config.floating_modifier = $<number>3;
        }
        ;

new_container:
        TOKNEWCONTAINER WHITESPACE TOKCONTAINERMODE
        {
                DLOG("new containers will be in mode %d\n", $<number>3);
                config.container_mode = $<number>3;

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
        }
        | TOKNEWCONTAINER WHITESPACE TOKSTACKLIMIT WHITESPACE TOKSTACKLIMIT WHITESPACE NUMBER
        {
                DLOG("stack-limit %d with val %d\n", $<number>5, $<number>7);
                config.container_stack_limit = $<number>5;
                config.container_stack_limit_value = $<number>7;

                /* See the comment above */
                Workspace *ws;
                TAILQ_FOREACH(ws, workspaces, workspaces) {
                        if (ws->table == NULL)
                                continue;
                        Container *con = ws->table[0][0];
                        con->stack_limit = config.container_stack_limit;
                        con->stack_limit_value = config.container_stack_limit_value;
                }
        }
        ;

new_window:
        TOKNEWWINDOW WHITESPACE WORD
        {
                DLOG("new windows should start in mode %s\n", $<string>3);
                config.default_border = strdup($<string>3);
        }
        ;

focus_follows_mouse:
        TOKFOCUSFOLLOWSMOUSE WHITESPACE NUMBER
        {
                config.focus_follows_mouse = ($<number>3 == 0 ? 0 : 1);
        }
        ;

workspace:
        TOKWORKSPACE WHITESPACE NUMBER WHITESPACE TOKSCREEN WHITESPACE screen optional_workspace_name
        {
                int ws_num = $<number>3;
                if (ws_num < 1) {
                        DLOG("Invalid workspace assignment, workspace number %d out of range\n", ws_num);
                } else {
                        Workspace *ws = workspace_get(ws_num - 1);
                        ws->preferred_screen = sstrdup($<string>7);
                        if ($<string>8 != NULL)
                                workspace_set_name(ws, $<string>8);
                }
        }
        | TOKWORKSPACE WHITESPACE NUMBER WHITESPACE workspace_name
        {
                int ws_num = $<number>3;
                if (ws_num < 1) {
                        DLOG("Invalid workspace assignment, workspace number %d out of range\n", ws_num);
                } else {
                        if ($<string>5 != NULL)
                                workspace_set_name(workspace_get(ws_num - 1), $<string>5);
                }
        }
        ;

optional_workspace_name:
        /* empty */                     { $<string>$ = NULL; }
        | WHITESPACE workspace_name     { $<string>$ = $<string>2; }
        ;

workspace_name:
        QUOTEDSTRING         { $<string>$ = $<string>1; }
        | STR                { $<string>$ = $<string>1; }
        | WORD               { $<string>$ = $<string>1; }
        ;

screen:
        NUMBER              { asprintf(&$<string>$, "%d", $<number>1); }
        | NUMBER 'x'        { asprintf(&$<string>$, "%d", $<number>1); }
        | NUMBER 'x' NUMBER { asprintf(&$<string>$, "%dx%d", $<number>1, $<number>3); }
        | 'x' NUMBER        { asprintf(&$<string>$, "x%d", $<number>2); }
        ;

assign:
        TOKASSIGN WHITESPACE window_class WHITESPACE optional_arrow assign_target
        {
                printf("assignment of %s\n", $<string>3);

                struct Assignment *new = $<assignment>6;
                printf("  to %d\n", new->workspace);
                printf("  floating = %d\n", new->floating);
                new->windowclass_title = strdup($<string>3);
                TAILQ_INSERT_TAIL(&assignments, new, assignments);
        }
        ;

assign_target:
        NUMBER
        {
                struct Assignment *new = scalloc(sizeof(struct Assignment));
                new->workspace = $<number>1;
                new->floating = ASSIGN_FLOATING_NO;
                $<assignment>$ = new;
        }
        | '~'
        {
                struct Assignment *new = scalloc(sizeof(struct Assignment));
                new->floating = ASSIGN_FLOATING_ONLY;
                $<assignment>$ = new;
        }
        | '~' NUMBER
        {
                struct Assignment *new = scalloc(sizeof(struct Assignment));
                new->workspace = $<number>2;
                new->floating = ASSIGN_FLOATING;
                $<assignment>$ = new;
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
                config.ipc_socket_path = sstrdup($<string>3);
        }
        ;

exec:
        TOKEXEC WHITESPACE STR
        {
                struct Autostart *new = smalloc(sizeof(struct Autostart));
                new->command = sstrdup($<string>3);
                TAILQ_INSERT_TAIL(&autostarts, new, autostarts);
        }
        ;

terminal:
        TOKTERMINAL WHITESPACE STR
        {
                DLOG("The terminal option is DEPRECATED and has no effect. "
                    "Please remove it from your configuration file.");
        }
        ;

font:
        TOKFONT WHITESPACE STR
        {
                config.font = sstrdup($<string>3);
                printf("font %s\n", config.font);
        }
        ;


color:
        TOKCOLOR WHITESPACE colorpixel WHITESPACE colorpixel WHITESPACE colorpixel
        {
                struct Colortriple *dest = $<color>1;

                dest->border = $<number>3;
                dest->background = $<number>5;
                dest->text = $<number>7;
        }
        ;

colorpixel:
        '#' HEX
        {
                char *hex;
                if (asprintf(&hex, "#%s", $<string>2) == -1)
                        die("asprintf()");
                $<number>$ = get_colorpixel(global_conn, hex);
                free(hex);
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
        | TOKCONTROL    { $<number>$ = BIND_CONTROL; }
        | TOKSHIFT      { $<number>$ = BIND_SHIFT; }
        ;
