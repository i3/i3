%{
/*
 * vim:ts=4:sw=4:expandtab
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "all.h"

static pid_t configerror_pid = -1;

static Match current_match;

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
    context->has_errors = true;

    ELOG("\n");
    ELOG("CONFIG: %s\n", error_message);
    ELOG("CONFIG: in file \"%s\", line %d:\n",
        context->filename, context->line_number);
    ELOG("CONFIG:   %s\n", context->line_copy);
    char buffer[context->last_column+1];
    buffer[context->last_column] = '\0';
    for (int c = 1; c <= context->last_column; c++)
        buffer[c-1] = (c >= context->first_column ? '^' : ' ');
    ELOG("CONFIG:   %s\n", buffer);
    ELOG("\n");
}

int yywrap() {
    return 1;
}

/*
 * Goes through each line of buf (separated by \n) and checks for statements /
 * commands which only occur in i3 v4 configuration files. If it finds any, it
 * returns version 4, otherwise it returns version 3.
 *
 */
static int detect_version(char *buf) {
    char *walk = buf;
    char *line = buf;
    while (*walk != '\0') {
        if (*walk != '\n') {
            walk++;
            continue;
        }

        /* check for some v4-only statements */
        if (strncasecmp(line, "bindcode", strlen("bindcode")) == 0 ||
            strncasecmp(line, "force_focus_wrapping", strlen("force_focus_wrapping")) == 0 ||
            strncasecmp(line, "# i3 config file (v4)", strlen("# i3 config file (v4)")) == 0 ||
            strncasecmp(line, "workspace_layout", strlen("workspace_layout")) == 0) {
            printf("deciding for version 4 due to this line: %.*s\n", (int)(walk-line), line);
            return 4;
        }

        /* if this is a bind statement, we can check the command */
        if (strncasecmp(line, "bind", strlen("bind")) == 0) {
            char *bind = strchr(line, ' ');
            if (bind == NULL)
                goto next;
            while ((*bind == ' ' || *bind == '\t') && *bind != '\0')
                bind++;
            if (*bind == '\0')
                goto next;
            if ((bind = strchr(bind, ' ')) == NULL)
                goto next;
            while ((*bind == ' ' || *bind == '\t') && *bind != '\0')
                bind++;
            if (*bind == '\0')
                goto next;
            if (strncasecmp(bind, "layout", strlen("layout")) == 0 ||
                strncasecmp(bind, "floating", strlen("floating")) == 0 ||
                strncasecmp(bind, "workspace", strlen("workspace")) == 0 ||
                strncasecmp(bind, "focus left", strlen("focus left")) == 0 ||
                strncasecmp(bind, "focus right", strlen("focus right")) == 0 ||
                strncasecmp(bind, "focus up", strlen("focus up")) == 0 ||
                strncasecmp(bind, "focus down", strlen("focus down")) == 0 ||
                strncasecmp(bind, "border normal", strlen("border normal")) == 0 ||
                strncasecmp(bind, "border 1pixel", strlen("border 1pixel")) == 0 ||
                strncasecmp(bind, "border borderless", strlen("border borderless")) == 0) {
                printf("deciding for version 4 due to this line: %.*s\n", (int)(walk-line), line);
                return 4;
            }
        }

next:
        /* advance to the next line */
        walk++;
        line = walk;
    }

    return 3;
}

/*
 * Calls i3-migrate-config-to-v4.pl to migrate a configuration file (input
 * buffer).
 *
 * Returns the converted config file or NULL if there was an error (for
 * example the script could not be found in $PATH or the i3 executable’s
 * directory).
 *
 */
static char *migrate_config(char *input, off_t size) {
    int writepipe[2];
    int readpipe[2];

    if (pipe(writepipe) != 0 ||
        pipe(readpipe) != 0) {
        warn("migrate_config: Could not create pipes");
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        warn("Could not fork()");
        return NULL;
    }

    /* child */
    if (pid == 0) {
        /* close writing end of writepipe, connect reading side to stdin */
        close(writepipe[1]);
        dup2(writepipe[0], 0);

        /* close reading end of readpipe, connect writing side to stdout */
        close(readpipe[0]);
        dup2(readpipe[1], 1);

        static char *argv[] = {
            NULL, /* will be replaced by the executable path */
            NULL
        };
        exec_i3_utility("i3-migrate-config-to-v4.pl", argv);
    }

    /* parent */

    /* close reading end of the writepipe (connected to the script’s stdin) */
    close(writepipe[0]);

    /* write the whole config file to the pipe, the script will read everything
     * immediately */
    int written = 0;
    int ret;
    while (written < size) {
        if ((ret = write(writepipe[1], input + written, size - written)) < 0) {
            warn("Could not write to pipe");
            return NULL;
        }
        written += ret;
    }
    close(writepipe[1]);

    /* close writing end of the readpipe (connected to the script’s stdout) */
    close(readpipe[1]);

    /* read the script’s output */
    int conv_size = 65535;
    char *converted = malloc(conv_size);
    int read_bytes = 0;
    do {
        if (read_bytes == conv_size) {
            conv_size += 65535;
            converted = realloc(converted, conv_size);
        }
        ret = read(readpipe[0], converted + read_bytes, conv_size - read_bytes);
        if (ret == -1) {
            warn("Cannot read from pipe");
            return NULL;
        }
        read_bytes += ret;
    } while (ret > 0);

    /* get the returncode */
    int status;
    wait(&status);
    if (!WIFEXITED(status)) {
        fprintf(stderr, "Child did not terminate normally, using old config file (will lead to broken behaviour)\n");
        return NULL;
    }

    int returncode = WEXITSTATUS(status);
    if (returncode != 0) {
        fprintf(stderr, "Migration process exit code was != 0\n");
        if (returncode == 2) {
            fprintf(stderr, "could not start the migration script\n");
            /* TODO: script was not found. tell the user to fix his system or create a v4 config */
        } else if (returncode == 1) {
            fprintf(stderr, "This already was a v4 config. Please add the following line to your config file:\n");
            fprintf(stderr, "# i3 config file (v4)\n");
            /* TODO: nag the user with a message to include a hint for i3 in his config file */
        }
        return NULL;
    }

    return converted;
}

/*
 * Handler which will be called when we get a SIGCHLD for the nagbar, meaning
 * it exited (or could not be started, depending on the exit code).
 *
 */
static void nagbar_exited(EV_P_ ev_child *watcher, int revents) {
    ev_child_stop(EV_A_ watcher);
    if (!WIFEXITED(watcher->rstatus)) {
        fprintf(stderr, "ERROR: i3-nagbar did not exit normally.\n");
        return;
    }

    int exitcode = WEXITSTATUS(watcher->rstatus);
    printf("i3-nagbar process exited with status %d\n", exitcode);
    if (exitcode == 2) {
        fprintf(stderr, "ERROR: i3-nagbar could not be found. Is it correctly installed on your system?\n");
    }

    configerror_pid = -1;
}

/*
 * Starts an i3-nagbar process which alerts the user that his configuration
 * file contains one or more errors. Also offers two buttons: One to launch an
 * $EDITOR on the config file and another one to launch a $PAGER on the error
 * logfile.
 *
 */
static void start_configerror_nagbar(const char *config_path) {
    fprintf(stderr, "Would start i3-nagscreen now\n");
    configerror_pid = fork();
    if (configerror_pid == -1) {
        warn("Could not fork()");
        return;
    }

    /* child */
    if (configerror_pid == 0) {
        char *editaction,
             *pageraction;
        if (asprintf(&editaction, TERM_EMU " -e sh -c \"${EDITOR:-vi} \"%s\" && i3-msg reload\"", config_path) == -1)
            exit(1);
        if (asprintf(&pageraction, TERM_EMU " -e sh -c \"${PAGER:-less} \"%s\"\"", errorfilename) == -1)
            exit(1);
        char *argv[] = {
            NULL, /* will be replaced by the executable path */
            "-m",
            "You have an error in your i3 config file!",
            "-b",
            "edit config",
            editaction,
            (errorfilename ? "-b" : NULL),
            "show errors",
            pageraction,
            NULL
        };
        exec_i3_utility("i3-nagbar", argv);
    }

    /* parent */
    /* install a child watcher */
    ev_child *child = smalloc(sizeof(ev_child));
    ev_child_init(child, &nagbar_exited, configerror_pid, 0);
    ev_child_start(main_loop, child);
}

/*
 * Kills the configerror i3-nagbar process, if any.
 *
 * Called when reloading/restarting.
 *
 * If wait_for_it is set (restarting), this function will waitpid(), otherwise,
 * ev is assumed to handle it (reloading).
 *
 */
void kill_configerror_nagbar(bool wait_for_it) {
    if (configerror_pid == -1)
        return;

    if (kill(configerror_pid, SIGTERM) == -1)
        warn("kill(configerror_nagbar) failed");

    if (!wait_for_it)
        return;

    /* When restarting, we don’t enter the ev main loop anymore and after the
     * exec(), our old pid is no longer watched. So, ev won’t handle SIGCHLD
     * for us and we would end up with a <defunct> process. Therefore we
     * waitpid() here. */
    waitpid(configerror_pid, NULL, 0);
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
    close(fd);

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

    /* analyze the string to find out whether this is an old config file (3.x)
     * or a new config file (4.x). If it’s old, we run the converter script. */
    int version = detect_version(buf);
    if (version == 3) {
        /* We need to convert this v3 configuration */
        char *converted = migrate_config(new, stbuf.st_size);
        if (converted != NULL) {
            printf("\n");
            printf("****************************************************************\n");
            printf("NOTE: Automatically converted configuration file from v3 to v4.\n");
            printf("\n");
            printf("Please convert your config file to v4. You can use this command:\n");
            printf("    mv %s %s.O\n", f, f);
            printf("    i3-migrate-config-to-v4.pl %s.O > %s\n", f, f);
            printf("****************************************************************\n");
            printf("\n");
            free(new);
            new = converted;
        } else {
            printf("\n");
            printf("**********************************************************************\n");
            printf("ERROR: Could not convert config file. Maybe i3-migrate-config-to-v4.pl\n");
            printf("was not correctly installed on your system?\n");
            printf("**********************************************************************\n");
            printf("\n");
        }
    }

    /* now lex/parse it */
    yy_scan_string(new);

    context = scalloc(sizeof(struct context));
    context->filename = f;

    if (yyparse() != 0) {
        fprintf(stderr, "Could not parse configfile\n");
        exit(1);
    }

    if (context->has_errors) {
        start_configerror_nagbar(f);
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
%token                  TOKFLOATING_MODIFIER        "floating_modifier"
%token  <string>        QUOTEDSTRING                "<quoted string>"
%token                  TOKWORKSPACE                "workspace"
%token                  TOKOUTPUT                   "output"
%token                  TOKASSIGN                   "assign"
%token                  TOKSET
%token                  TOKIPCSOCKET                "ipc_socket"
%token                  TOKRESTARTSTATE             "restart_state"
%token                  TOKEXEC                     "exec"
%token                  TOKEXEC_ALWAYS              "exec_always"
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
%token                  TOK_FORCE_FOCUS_WRAPPING    "force_focus_wrapping"
%token                  TOKWORKSPACEBAR             "workspace_bar"
%token                  TOK_DEFAULT                 "default"
%token                  TOK_STACKING                "stacking"
%token                  TOK_TABBED                  "tabbed"
%token  <number>        TOKSTACKLIMIT               "stack-limit"
%token                  TOK_POPUP_DURING_FULLSCREEN "popup_during_fullscreen"
%token                  TOK_IGNORE                  "ignore"
%token                  TOK_LEAVE_FULLSCREEN        "leave_fullscreen"
%token                  TOK_FOR_WINDOW              "for_window"

%token              TOK_MARK            "mark"
%token              TOK_CLASS           "class"
%token              TOK_ID              "id"
%token              TOK_CON_ID          "con_id"
%token              TOK_TITLE           "title"

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

%%

lines: /* empty */
    | lines error
    | lines line
    ;

line:
    bindline
    | for_window
    | mode
    | floating_modifier
    | orientation
    | workspace_layout
    | new_window
    | focus_follows_mouse
    | force_focus_wrapping
    | workspace_bar
    | workspace
    | assign
    | ipcsocket
    | restart_state
    | exec
    | exec_always
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
    TOKBINDCODE bindcode         { $$ = $2; }
    | TOKBINDSYM bindsym         { $$ = $2; }
    ;

bindcode:
    binding_modifiers NUMBER command
    {
        printf("\tFound keycode binding mod%d with key %d and command %s\n", $1, $2, $3);
        Binding *new = scalloc(sizeof(Binding));

        new->keycode = $2;
        new->mods = $1;
        new->command = $3;

        $$ = new;
    }
    ;

bindsym:
    binding_modifiers word_or_number command
    {
        printf("\tFound keysym binding mod%d with key %s and command %s\n", $1, $2, $3);
        Binding *new = scalloc(sizeof(Binding));

        new->symbol = $2;
        new->mods = $1;
        new->command = $3;

        $$ = new;
    }
    ;

for_window:
    TOK_FOR_WINDOW match command
    {
        printf("\t should execute command %s for the criteria mentioned above\n", $3);
        Assignment *assignment = scalloc(sizeof(Assignment));
        assignment->type = A_COMMAND;
        assignment->match = current_match;
        assignment->dest.command = $3;
        TAILQ_INSERT_TAIL(&assignments, assignment, assignments);
    }
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
    }
    ;

matchend:
    ']'
    {
        printf("match specification finished\n");
    }
    ;

criteria:
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



word_or_number:
    WORD
    | NUMBER
    {
        asprintf(&$$, "%d", $1);
    }
    ;

mode:
    TOKMODE QUOTEDSTRING '{' modelines '}'
    {
        if (strcasecmp($2, "default") == 0) {
            printf("You cannot use the name \"default\" for your mode\n");
            exit(1);
        }
        printf("\t now in mode %s\n", $2);
        printf("\t current bindings = %p\n", current_bindings);
        Binding *binding;
        TAILQ_FOREACH(binding, current_bindings, bindings) {
            printf("got binding on mods %d, keycode %d, symbol %s, command %s\n",
                            binding->mods, binding->keycode, binding->symbol, binding->command);
        }

        struct Mode *mode = scalloc(sizeof(struct Mode));
        mode->name = $2;
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
    comment
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
    TOKFLOATING_MODIFIER binding_modifiers
    {
        DLOG("floating modifier = %d\n", $2);
        config.floating_modifier = $2;
    }
    ;

orientation:
    TOK_ORIENTATION direction
    {
        DLOG("New containers should start with split direction %d\n", $2);
        config.default_orientation = $2;
    }
    ;

direction:
    TOK_HORIZ       { $$ = HORIZ; }
    | TOK_VERT      { $$ = VERT; }
    | TOK_AUTO      { $$ = NO_ORIENTATION; }
    ;

workspace_layout:
    TOK_WORKSPACE_LAYOUT layout_mode
    {
        DLOG("new containers will be in mode %d\n", $2);
        config.default_layout = $2;

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
    | TOK_WORKSPACE_LAYOUT TOKSTACKLIMIT TOKSTACKLIMIT NUMBER
    {
        DLOG("stack-limit %d with val %d\n", $3, $4);
        config.container_stack_limit = $3;
        config.container_stack_limit_value = $4;

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
    TOKNEWWINDOW border_style
    {
        DLOG("new windows should start with border style %d\n", $2);
        config.default_border = $2;
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
    TOKFOCUSFOLLOWSMOUSE bool
    {
        DLOG("focus follows mouse = %d\n", $2);
        config.disable_focus_follows_mouse = !($2);
    }
    ;

force_focus_wrapping:
    TOK_FORCE_FOCUS_WRAPPING bool
    {
        DLOG("force focus wrapping = %d\n", $2);
        config.force_focus_wrapping = $2;
    }
    ;

workspace_bar:
    TOKWORKSPACEBAR bool
    {
        DLOG("workspace bar = %d\n", $2);
        config.disable_workspace_bar = !($2);
    }
    ;

workspace:
    TOKWORKSPACE NUMBER TOKOUTPUT OUTPUT optional_workspace_name
    {
        int ws_num = $2;
        if (ws_num < 1) {
            DLOG("Invalid workspace assignment, workspace number %d out of range\n", ws_num);
        } else {
            char *ws_name = NULL;
            if ($5 == NULL) {
                asprintf(&ws_name, "%d", ws_num);
            } else {
                ws_name = $5;
            }

            DLOG("Should assign workspace %s to output %s\n", ws_name, $4);
            struct Workspace_Assignment *assignment = scalloc(sizeof(struct Workspace_Assignment));
            assignment->name = ws_name;
            assignment->output = $4;
            TAILQ_INSERT_TAIL(&ws_assignments, assignment, ws_assignments);
        }
    }
    | TOKWORKSPACE NUMBER workspace_name
    {
        int ws_num = $2;
        if (ws_num < 1) {
            DLOG("Invalid workspace assignment, workspace number %d out of range\n", ws_num);
        } else {
            DLOG("workspace name to: %s\n", $3);
#if 0
            if ($<string>3 != NULL) {
                    workspace_set_name(workspace_get(ws_num - 1), $<string>3);
                    free($<string>3);
            }
#endif
        }
    }
    ;

optional_workspace_name:
    /* empty */          { $$ = NULL; }
    | workspace_name     { $$ = $1; }
    ;

workspace_name:
    QUOTEDSTRING         { $$ = $1; }
    | STR                { $$ = $1; }
    | WORD               { $$ = $1; }
    ;

assign:
    TOKASSIGN window_class STR
    {
        printf("assignment of %s to *%s*\n", $2, $3);
        char *workspace = $3;
        char *criteria = $2;

        Assignment *assignment = scalloc(sizeof(Assignment));
        Match *match = &(assignment->match);
        match_init(match);

        char *separator = NULL;
        if ((separator = strchr(criteria, '/')) != NULL) {
            *(separator++) = '\0';
            match->title = sstrdup(separator);
        }
        if (*criteria != '\0')
            match->class = sstrdup(criteria);
        free(criteria);

        printf("  class = %s\n", match->class);
        printf("  title = %s\n", match->title);

        /* Compatibility with older versions: If the assignment target starts
         * with ~, we create the equivalent of:
         *
         * for_window [class="foo"] mode floating
         */
        if (*workspace == '~') {
            workspace++;
            if (*workspace == '\0') {
                /* This assignment was *only* for floating */
                assignment->type = A_COMMAND;
                assignment->dest.command = sstrdup("floating enable");
                TAILQ_INSERT_TAIL(&assignments, assignment, assignments);
                break;
            } else {
                /* Create a new assignment and continue afterwards */
                Assignment *floating = scalloc(sizeof(Assignment));
                match_copy(&(floating->match), match);
                floating->type = A_COMMAND;
                floating->dest.command = sstrdup("floating enable");
                TAILQ_INSERT_TAIL(&assignments, floating, assignments);
            }
        }

        assignment->type = A_TO_WORKSPACE;
        assignment->dest.workspace = workspace;
        TAILQ_INSERT_TAIL(&assignments, assignment, assignments);
    }
    ;

window_class:
    QUOTEDSTRING
    | STR_NG
    ;

ipcsocket:
    TOKIPCSOCKET STR
    {
        config.ipc_socket_path = $2;
    }
    ;

restart_state:
    TOKRESTARTSTATE STR
    {
        config.restart_state_path = $2;
    }
    ;

exec:
    TOKEXEC STR
    {
        struct Autostart *new = smalloc(sizeof(struct Autostart));
        new->command = $2;
        TAILQ_INSERT_TAIL(&autostarts, new, autostarts);
    }
    ;

exec_always:
    TOKEXEC_ALWAYS STR
    {
        struct Autostart *new = smalloc(sizeof(struct Autostart));
        new->command = $2;
        TAILQ_INSERT_TAIL(&autostarts_always, new, autostarts_always);
    }
    ;

terminal:
    TOKTERMINAL STR
    {
        ELOG("The terminal option is DEPRECATED and has no effect. "
            "Please remove it from your configuration file.\n");
    }
    ;

font:
    TOKFONT STR
    {
        config.font = load_font($2, true);
        printf("font %s\n", $2);
    }
    ;

single_color:
    TOKSINGLECOLOR colorpixel
    {
        uint32_t *dest = $1;
        *dest = $2;
    }
    ;

color:
    TOKCOLOR colorpixel colorpixel colorpixel
    {
        struct Colortriple *dest = $1;

        dest->border = $2;
        dest->background = $3;
        dest->text = $4;
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
    TOK_POPUP_DURING_FULLSCREEN popup_setting
    {
        DLOG("popup_during_fullscreen setting: %d\n", $2);
        config.popup_during_fullscreen = $2;
    }
    ;

popup_setting:
    TOK_IGNORE              { $$ = PDF_IGNORE; }
    | TOK_LEAVE_FULLSCREEN  { $$ = PDF_LEAVE_FULLSCREEN; }
    ;
