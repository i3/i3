/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * i3-config-wizard: Program to convert configs using keycodes to configs using
 *                   keysyms.
 *
 */
#if defined(__FreeBSD__)
#include <sys/param.h>
#endif

/* For systems without getline, fall back to fgetln */
#if defined(__APPLE__)
#define USE_FGETLN
#elif defined(__FreeBSD__)
/* Defining this macro before including stdio.h is necessary in order to have
 * a prototype for getline in FreeBSD. */
#define _WITH_GETLINE
#endif

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <err.h>
#include <stdint.h>
#include <getopt.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glob.h>
#include <assert.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_keysyms.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>

/* We need SYSCONFDIR for the path to the keycode config template, so raise an
 * error if it’s not defined for whatever reason */
#ifndef SYSCONFDIR
#error "SYSCONFDIR not defined"
#endif

#define FREE(pointer)          \
    do {                       \
        if (pointer != NULL) { \
            free(pointer);     \
            pointer = NULL;    \
        }                      \
    } while (0)

#include "xcb.h"
#include "libi3.h"

#define row_y(row) \
    (((row)-1) * font.height + logical_px(4))
#define window_height() \
    (row_y(15) + font.height)

enum { STEP_WELCOME,
       STEP_GENERATE } current_step = STEP_WELCOME;
enum { MOD_Mod1,
       MOD_Mod4 } modifier = MOD_Mod4;

static char *config_path;
static uint32_t xcb_numlock_mask;
xcb_connection_t *conn;
static xcb_key_symbols_t *keysyms;
xcb_screen_t *root_screen;
static xcb_get_modifier_mapping_reply_t *modmap_reply;
static i3Font font;
static i3Font bold_font;
static int char_width;
static char *socket_path;
static xcb_window_t win;
static xcb_pixmap_t pixmap;
static xcb_gcontext_t pixmap_gc;
static xcb_key_symbols_t *symbols;
xcb_window_t root;
static struct xkb_keymap *xkb_keymap;
static uint8_t xkb_base_event;
static uint8_t xkb_base_error;

static void finish();

#include "GENERATED_config_enums.h"

typedef struct token {
    char *name;
    char *identifier;
    /* This might be __CALL */
    cmdp_state next_state;
    union {
        uint16_t call_identifier;
    } extra;
} cmdp_token;

typedef struct tokenptr {
    cmdp_token *array;
    int n;
} cmdp_token_ptr;

#include "GENERATED_config_tokens.h"

static cmdp_state state;
/* A list which contains the states that lead to the current state, e.g.
 * INITIAL, WORKSPACE_LAYOUT.
 * When jumping back to INITIAL, statelist_idx will simply be set to 1
 * (likewise for other states, e.g. MODE or BAR).
 * This list is used to process the nearest error token. */
static cmdp_state statelist[10] = {INITIAL};
/* NB: statelist_idx points to where the next entry will be inserted */
static int statelist_idx = 1;

struct stack_entry {
    /* Just a pointer, not dynamically allocated. */
    const char *identifier;
    enum {
        STACK_STR = 0,
        STACK_LONG = 1,
    } type;
    union {
        char *str;
        long num;
    } val;
};

/* 10 entries should be enough for everybody. */
static struct stack_entry stack[10];

/*
 * Pushes a string (identified by 'identifier') on the stack. We simply use a
 * single array, since the number of entries we have to store is very small.
 *
 */
static void push_string(const char *identifier, const char *str) {
    for (int c = 0; c < 10; c++) {
        if (stack[c].identifier != NULL &&
            strcmp(stack[c].identifier, identifier) != 0)
            continue;
        if (stack[c].identifier == NULL) {
            /* Found a free slot, let’s store it here. */
            stack[c].identifier = identifier;
            stack[c].val.str = sstrdup(str);
            stack[c].type = STACK_STR;
        } else {
            /* Append the value. */
            char *prev = stack[c].val.str;
            sasprintf(&(stack[c].val.str), "%s,%s", prev, str);
            free(prev);
        }
        return;
    }

    /* When we arrive here, the stack is full. This should not happen and
     * means there’s either a bug in this parser or the specification
     * contains a command with more than 10 identified tokens. */
    fprintf(stderr, "BUG: commands_parser stack full. This means either a bug "
                    "in the code, or a new command which contains more than "
                    "10 identified tokens.\n");
    exit(1);
}

static void push_long(const char *identifier, long num) {
    for (int c = 0; c < 10; c++) {
        if (stack[c].identifier != NULL)
            continue;
        /* Found a free slot, let’s store it here. */
        stack[c].identifier = identifier;
        stack[c].val.num = num;
        stack[c].type = STACK_LONG;
        return;
    }

    /* When we arrive here, the stack is full. This should not happen and
     * means there’s either a bug in this parser or the specification
     * contains a command with more than 10 identified tokens. */
    fprintf(stderr, "BUG: commands_parser stack full. This means either a bug "
                    "in the code, or a new command which contains more than "
                    "10 identified tokens.\n");
    exit(1);
}

static const char *get_string(const char *identifier) {
    for (int c = 0; c < 10; c++) {
        if (stack[c].identifier == NULL)
            break;
        if (strcmp(identifier, stack[c].identifier) == 0)
            return stack[c].val.str;
    }
    return NULL;
}

static void clear_stack(void) {
    for (int c = 0; c < 10; c++) {
        if (stack[c].type == STACK_STR && stack[c].val.str != NULL)
            free(stack[c].val.str);
        stack[c].identifier = NULL;
        stack[c].val.str = NULL;
        stack[c].val.num = 0;
    }
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

static char *next_state(const cmdp_token *token) {
    cmdp_state _next_state = token->next_state;

    if (token->next_state == __CALL) {
        const char *modifiers = get_string("modifiers");
        int keycode = atoi(get_string("key"));
        int level = 0;
        if (modifiers != NULL &&
            strstr(modifiers, "Shift") != NULL) {
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
            const xkb_keysym_t *syms;
            int num = xkb_keymap_key_get_syms_by_level(xkb_keymap, keycode, 0, 0, &syms);
            if (num == 0)
                errx(1, "xkb_keymap_key_get_syms_by_level returned no symbols for keycode %d", keycode);
            if (!keysym_used_on_other_key(syms[0], keycode))
                level = 0;
        }

        const xkb_keysym_t *syms;
        int num = xkb_keymap_key_get_syms_by_level(xkb_keymap, keycode, 0, level, &syms);
        if (num == 0)
            errx(1, "xkb_keymap_key_get_syms_by_level returned no symbols for keycode %d", keycode);
        if (num > 1)
            printf("xkb_keymap_key_get_syms_by_level (keycode = %d) returned %d symbolsinstead of 1, using only the first one.\n", keycode, num);

        char str[4096];
        if (xkb_keysym_get_name(syms[0], str, sizeof(str)) == -1)
            errx(EXIT_FAILURE, "xkb_keysym_get_name(%u) failed", syms[0]);
        const char *release = get_string("release");
        char *res;
        char *modrep = (modifiers == NULL ? sstrdup("") : sstrdup(modifiers));
        char *comma;
        while ((comma = strchr(modrep, ',')) != NULL) {
            *comma = '+';
        }
        sasprintf(&res, "bindsym %s%s%s %s%s\n", (modifiers == NULL ? "" : modrep), (modifiers == NULL ? "" : "+"), str, (release == NULL ? "" : release), get_string("command"));
        clear_stack();
        return res;
    }

    state = _next_state;

    /* See if we are jumping back to a state in which we were in previously
     * (statelist contains INITIAL) and just move statelist_idx accordingly. */
    for (int i = 0; i < statelist_idx; i++) {
        if (statelist[i] != _next_state)
            continue;
        statelist_idx = i + 1;
        return NULL;
    }

    /* Otherwise, the state is new and we add it to the list */
    statelist[statelist_idx++] = _next_state;
    return NULL;
}

static char *rewrite_binding(const char *input) {
    state = INITIAL;
    statelist_idx = 1;

    const char *walk = input;
    const size_t len = strlen(input);
    int c;
    const cmdp_token *token;
    char *result = NULL;

    /* The "<=" operator is intentional: We also handle the terminating 0-byte
     * explicitly by looking for an 'end' token. */
    while ((size_t)(walk - input) <= len) {
        /* Skip whitespace before every token, newlines are relevant since they
         * separate configuration directives. */
        while ((*walk == ' ' || *walk == '\t') && *walk != '\0')
            walk++;

        //printf("remaining input: %s\n", walk);

        cmdp_token_ptr *ptr = &(tokens[state]);
        for (c = 0; c < ptr->n; c++) {
            token = &(ptr->array[c]);

            /* A literal. */
            if (token->name[0] == '\'') {
                if (strncasecmp(walk, token->name + 1, strlen(token->name) - 1) == 0) {
                    if (token->identifier != NULL)
                        push_string(token->identifier, token->name + 1);
                    walk += strlen(token->name) - 1;
                    if ((result = next_state(token)) != NULL)
                        return result;
                    break;
                }
                continue;
            }

            if (strcmp(token->name, "number") == 0) {
                /* Handle numbers. We only accept decimal numbers for now. */
                char *end = NULL;
                errno = 0;
                long int num = strtol(walk, &end, 10);
                if ((errno == ERANGE && (num == LONG_MIN || num == LONG_MAX)) ||
                    (errno != 0 && num == 0))
                    continue;

                /* No valid numbers found */
                if (end == walk)
                    continue;

                if (token->identifier != NULL)
                    push_long(token->identifier, num);

                /* Set walk to the first non-number character */
                walk = end;
                if ((result = next_state(token)) != NULL)
                    return result;
                break;
            }

            if (strcmp(token->name, "string") == 0 ||
                strcmp(token->name, "word") == 0) {
                const char *beginning = walk;
                /* Handle quoted strings (or words). */
                if (*walk == '"') {
                    beginning++;
                    walk++;
                    while (*walk != '\0' && (*walk != '"' || *(walk - 1) == '\\'))
                        walk++;
                } else {
                    if (token->name[0] == 's') {
                        while (*walk != '\0' && *walk != '\r' && *walk != '\n')
                            walk++;
                    } else {
                        /* For a word, the delimiters are white space (' ' or
                         * '\t'), closing square bracket (]), comma (,) and
                         * semicolon (;). */
                        while (*walk != ' ' && *walk != '\t' &&
                               *walk != ']' && *walk != ',' &&
                               *walk != ';' && *walk != '\r' &&
                               *walk != '\n' && *walk != '\0')
                            walk++;
                    }
                }
                if (walk != beginning) {
                    char *str = scalloc(walk - beginning + 1);
                    /* We copy manually to handle escaping of characters. */
                    int inpos, outpos;
                    for (inpos = 0, outpos = 0;
                         inpos < (walk - beginning);
                         inpos++, outpos++) {
                        /* We only handle escaped double quotes to not break
                         * backwards compatibility with people using \w in
                         * regular expressions etc. */
                        if (beginning[inpos] == '\\' && beginning[inpos + 1] == '"')
                            inpos++;
                        str[outpos] = beginning[inpos];
                    }
                    if (token->identifier)
                        push_string(token->identifier, str);
                    free(str);
                    /* If we are at the end of a quoted string, skip the ending
                     * double quote. */
                    if (*walk == '"')
                        walk++;
                    if ((result = next_state(token)) != NULL)
                        return result;
                    break;
                }
            }

            if (strcmp(token->name, "end") == 0) {
                //printf("checking for end: *%s*\n", walk);
                if (*walk == '\0' || *walk == '\n' || *walk == '\r') {
                    if ((result = next_state(token)) != NULL)
                        return result;
                    /* To make sure we start with an appropriate matching
                     * datastructure for commands which do *not* specify any
                     * criteria, we re-initialize the criteria system after
                     * every command. */
                    // TODO: make this testable
                    walk++;
                    break;
                }
            }
        }
    }

    return NULL;
}

/*
 * Having verboselog(), errorlog() and debuglog() is necessary when using libi3.
 *
 */
void verboselog(char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
}

void errorlog(char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

void debuglog(char *fmt, ...) {
}

/*
 * Handles expose events, that is, draws the window contents.
 *
 */
static int handle_expose() {
    /* re-draw the background */
    xcb_rectangle_t border = {0, 0, logical_px(300), window_height()};
    xcb_change_gc(conn, pixmap_gc, XCB_GC_FOREGROUND, (uint32_t[]){get_colorpixel("#000000")});
    xcb_poly_fill_rectangle(conn, pixmap, pixmap_gc, 1, &border);

    set_font(&font);

#define txt(x, row, text)                    \
    draw_text_ascii(text, pixmap, pixmap_gc, \
                    x, row_y(row), logical_px(500) - x * 2)

    if (current_step == STEP_WELCOME) {
        /* restore font color */
        set_font_colors(pixmap_gc, get_colorpixel("#FFFFFF"), get_colorpixel("#000000"));

        txt(logical_px(10), 2, "You have not configured i3 yet.");
        txt(logical_px(10), 3, "Do you want me to generate a config at");

        char *msg;
        sasprintf(&msg, "%s?", config_path);
        txt(logical_px(10), 4, msg);
        free(msg);

        txt(logical_px(85), 6, "Yes, generate the config");
        txt(logical_px(85), 8, "No, I will use the defaults");

        /* green */
        set_font_colors(pixmap_gc, get_colorpixel("#00FF00"), get_colorpixel("#000000"));
        txt(logical_px(25), 6, "<Enter>");

        /* red */
        set_font_colors(pixmap_gc, get_colorpixel("#FF0000"), get_colorpixel("#000000"));
        txt(logical_px(31), 8, "<ESC>");
    }

    if (current_step == STEP_GENERATE) {
        set_font_colors(pixmap_gc, get_colorpixel("#FFFFFF"), get_colorpixel("#000000"));

        txt(logical_px(10), 2, "Please choose either:");
        txt(logical_px(85), 4, "Win as default modifier");
        txt(logical_px(85), 5, "Alt as default modifier");
        txt(logical_px(10), 7, "Afterwards, press");
        txt(logical_px(85), 9, "to write the config");
        txt(logical_px(85), 10, "to abort");

        /* the not-selected modifier */
        if (modifier == MOD_Mod4)
            txt(logical_px(31), 5, "<Alt>");
        else
            txt(logical_px(31), 4, "<Win>");

        /* the selected modifier */
        set_font(&bold_font);
        set_font_colors(pixmap_gc, get_colorpixel("#FFFFFF"), get_colorpixel("#000000"));
        if (modifier == MOD_Mod4)
            txt(logical_px(10), 4, "-> <Win>");
        else
            txt(logical_px(10), 5, "-> <Alt>");

        /* green */
        set_font(&font);
        set_font_colors(pixmap_gc, get_colorpixel("#00FF00"), get_colorpixel("#000000"));
        txt(logical_px(25), 9, "<Enter>");

        /* red */
        set_font_colors(pixmap_gc, get_colorpixel("#FF0000"), get_colorpixel("#000000"));
        txt(logical_px(31), 10, "<ESC>");
    }

    /* Copy the contents of the pixmap to the real window */
    xcb_copy_area(conn, pixmap, win, pixmap_gc, 0, 0, 0, 0, logical_px(500), logical_px(500));
    xcb_flush(conn);

    return 1;
}

static int handle_key_press(void *ignored, xcb_connection_t *conn, xcb_key_press_event_t *event) {
    printf("Keypress %d, state raw = %d\n", event->detail, event->state);

    /* Remove the numlock bit, all other bits are modifiers we can bind to */
    uint16_t state_filtered = event->state & ~(xcb_numlock_mask | XCB_MOD_MASK_LOCK);
    /* Only use the lower 8 bits of the state (modifier masks) so that mouse
     * button masks are filtered out */
    state_filtered &= 0xFF;

    xcb_keysym_t sym = xcb_key_press_lookup_keysym(symbols, event, state_filtered);

    printf("sym = %c (%d)\n", sym, sym);

    if (sym == XK_Return || sym == XK_KP_Enter) {
        if (current_step == STEP_WELCOME) {
            current_step = STEP_GENERATE;
            /* Set window title */
            xcb_change_property(conn,
                                XCB_PROP_MODE_REPLACE,
                                win,
                                A__NET_WM_NAME,
                                A_UTF8_STRING,
                                8,
                                strlen("i3: generate config"),
                                "i3: generate config");
            xcb_flush(conn);
        } else
            finish();
    }

    /* Swap between modifiers when up or down is pressed. */
    if (sym == XK_Up || sym == XK_Down) {
        modifier = (modifier == MOD_Mod1) ? MOD_Mod4 : MOD_Mod1;
        handle_expose();
    }

    /* cancel any time */
    if (sym == XK_Escape)
        exit(0);

    /* Check if this is Mod1 or Mod4. The modmap contains Shift, Lock, Control,
     * Mod1, Mod2, Mod3, Mod4, Mod5 (in that order) */
    xcb_keycode_t *modmap = xcb_get_modifier_mapping_keycodes(modmap_reply);
    /* Mod1? */
    int mask = 3;
    for (int i = 0; i < modmap_reply->keycodes_per_modifier; i++) {
        xcb_keycode_t code = modmap[(mask * modmap_reply->keycodes_per_modifier) + i];
        if (code == XCB_NONE)
            continue;
        printf("Modifier keycode for Mod1: 0x%02x\n", code);
        if (code == event->detail) {
            modifier = MOD_Mod1;
            printf("This is Mod1!\n");
        }
    }

    /* Mod4? */
    mask = 6;
    for (int i = 0; i < modmap_reply->keycodes_per_modifier; i++) {
        xcb_keycode_t code = modmap[(mask * modmap_reply->keycodes_per_modifier) + i];
        if (code == XCB_NONE)
            continue;
        printf("Modifier keycode for Mod4: 0x%02x\n", code);
        if (code == event->detail) {
            modifier = MOD_Mod4;
            printf("This is Mod4!\n");
        }
    }

    handle_expose();
    return 1;
}

/*
 * Handle button presses to make clicking on "<win>" and "<alt>" work
 *
 */
static void handle_button_press(xcb_button_press_event_t *event) {
    if (current_step != STEP_GENERATE)
        return;

    if (event->event_x < logical_px(32) ||
        event->event_x > (logical_px(32) + char_width * 5))
        return;

    if (event->event_y >= row_y(4) && event->event_y <= (row_y(4) + font.height)) {
        modifier = MOD_Mod4;
        handle_expose();
    }

    if (event->event_y >= row_y(5) && event->event_y <= (row_y(5) + font.height)) {
        modifier = MOD_Mod1;
        handle_expose();
    }

    return;
}

/*
 * Creates the config file and tells i3 to reload.
 *
 */
static void finish() {
    printf("creating \"%s\"...\n", config_path);

    struct xkb_context *xkb_context;

    if ((xkb_context = xkb_context_new(0)) == NULL)
        errx(1, "could not create xkbcommon context");

    int32_t device_id = xkb_x11_get_core_keyboard_device_id(conn);
    if ((xkb_keymap = xkb_x11_keymap_new_from_device(xkb_context, conn, device_id, 0)) == NULL)
        errx(1, "xkb_x11_keymap_new_from_device failed");

    FILE *kc_config = fopen(SYSCONFDIR "/i3/config.keycodes", "r");
    if (kc_config == NULL)
        err(1, "Could not open input file \"%s\"", SYSCONFDIR "/i3/config.keycodes");

    FILE *ks_config = fopen(config_path, "w");
    if (ks_config == NULL)
        err(1, "Could not open output config file \"%s\"", config_path);
    free(config_path);

    char *line = NULL;
    size_t len = 0;
#ifndef USE_FGETLN
    ssize_t read;
#endif
    bool head_of_file = true;

    /* write a header about auto-generation to the output file */
    fputs("# This file has been auto-generated by i3-config-wizard(1).\n", ks_config);
    fputs("# It will not be overwritten, so edit it as you like.\n", ks_config);
    fputs("#\n", ks_config);
    fputs("# Should you change your keyboard layout some time, delete\n", ks_config);
    fputs("# this file and re-run i3-config-wizard(1).\n", ks_config);
    fputs("#\n", ks_config);

#ifdef USE_FGETLN
    char *buf = NULL;
    while ((buf = fgetln(kc_config, &len)) != NULL) {
        /* fgetln does not return null-terminated strings */
        FREE(line);
        sasprintf(&line, "%.*s", len, buf);
#else
    size_t linecap = 0;
    while ((read = getline(&line, &linecap, kc_config)) != -1) {
        len = strlen(line);
#endif
        /* skip the warning block at the beginning of the input file */
        if (head_of_file &&
            strncmp("# WARNING", line, strlen("# WARNING")) == 0)
            continue;

        head_of_file = false;

        /* Skip leading whitespace */
        char *walk = line;
        while (isspace(*walk) && walk < (line + len)) {
            /* Pre-output the skipped whitespaces to keep proper indentation */
            fputc(*walk, ks_config);
            walk++;
        }

        /* Set the modifier the user chose */
        if (strncmp(walk, "set $mod ", strlen("set $mod ")) == 0) {
            if (modifier == MOD_Mod1)
                fputs("set $mod Mod1\n", ks_config);
            else
                fputs("set $mod Mod4\n", ks_config);
            continue;
        }

        /* Check for 'bindcode'. If it’s not a bindcode line, we
         * just copy it to the output file */
        if (strncmp(walk, "bindcode", strlen("bindcode")) != 0) {
            fputs(walk, ks_config);
            continue;
        }
        char *result = rewrite_binding(walk);
        fputs(result, ks_config);
        free(result);
    }

    /* sync to do our best in order to have the file really stored on disk */
    fflush(ks_config);
    fsync(fileno(ks_config));

#ifndef USE_FGETLN
    free(line);
#endif

    fclose(kc_config);
    fclose(ks_config);

    /* tell i3 to reload the config file */
    int sockfd = ipc_connect(socket_path);
    ipc_send_message(sockfd, strlen("reload"), 0, (uint8_t *)"reload");
    close(sockfd);

    exit(0);
}

int main(int argc, char *argv[]) {
    char *xdg_config_home;
    socket_path = getenv("I3SOCK");
    char *pattern = "pango:monospace 8";
    char *patternbold = "pango:monospace bold 8";
    int o, option_index = 0;

    static struct option long_options[] = {
        {"socket", required_argument, 0, 's'},
        {"version", no_argument, 0, 'v'},
        {"limit", required_argument, 0, 'l'},
        {"prompt", required_argument, 0, 'P'},
        {"prefix", required_argument, 0, 'p'},
        {"font", required_argument, 0, 'f'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    char *options_string = "s:vh";

    while ((o = getopt_long(argc, argv, options_string, long_options, &option_index)) != -1) {
        switch (o) {
            case 's':
                FREE(socket_path);
                socket_path = strdup(optarg);
                break;
            case 'v':
                printf("i3-config-wizard " I3_VERSION "\n");
                return 0;
            case 'h':
                printf("i3-config-wizard " I3_VERSION "\n");
                printf("i3-config-wizard [-s <socket>] [-v]\n");
                return 0;
        }
    }

    char *path = get_config_path(NULL, false);
    if (path != NULL) {
        printf("The config file \"%s\" already exists. Exiting.\n", path);
        free(path);
        return 0;
    }

    /* Always write to $XDG_CONFIG_HOME/i3/config by default. */
    if ((xdg_config_home = getenv("XDG_CONFIG_HOME")) == NULL)
        xdg_config_home = "~/.config";

    xdg_config_home = resolve_tilde(xdg_config_home);
    sasprintf(&config_path, "%s/i3/config", xdg_config_home);

    /* Create $XDG_CONFIG_HOME/i3 if it does not yet exist */
    char *config_dir;
    struct stat stbuf;
    sasprintf(&config_dir, "%s/i3", xdg_config_home);
    if (stat(config_dir, &stbuf) != 0)
        if (!mkdirp(config_dir))
            err(EXIT_FAILURE, "mkdirp(%s) failed", config_dir);
    free(config_dir);
    free(xdg_config_home);

    int fd;
    if ((fd = open(config_path, O_CREAT | O_RDWR, 0644)) == -1) {
        printf("Cannot open file \"%s\" for writing: %s. Exiting.\n", config_path, strerror(errno));
        return 0;
    }
    close(fd);
    unlink(config_path);

    int screen;
    if ((conn = xcb_connect(NULL, &screen)) == NULL ||
        xcb_connection_has_error(conn))
        errx(1, "Cannot open display\n");

    if (xkb_x11_setup_xkb_extension(conn,
                                    XKB_X11_MIN_MAJOR_XKB_VERSION,
                                    XKB_X11_MIN_MINOR_XKB_VERSION,
                                    0,
                                    NULL,
                                    NULL,
                                    &xkb_base_event,
                                    &xkb_base_error) != 1)
        errx(EXIT_FAILURE, "Could not setup XKB extension.");

    if (socket_path == NULL)
        socket_path = root_atom_contents("I3_SOCKET_PATH", conn, screen);

    if (socket_path == NULL)
        socket_path = "/tmp/i3-ipc.sock";

    keysyms = xcb_key_symbols_alloc(conn);
    xcb_get_modifier_mapping_cookie_t modmap_cookie;
    modmap_cookie = xcb_get_modifier_mapping(conn);
    symbols = xcb_key_symbols_alloc(conn);

/* Place requests for the atoms we need as soon as possible */
#define xmacro(atom) \
    xcb_intern_atom_cookie_t atom##_cookie = xcb_intern_atom(conn, 0, strlen(#atom), #atom);
#include "atoms.xmacro"
#undef xmacro

    root_screen = xcb_aux_get_screen(conn, screen);
    root = root_screen->root;

    if (!(modmap_reply = xcb_get_modifier_mapping_reply(conn, modmap_cookie, NULL)))
        errx(EXIT_FAILURE, "Could not get modifier mapping\n");

    xcb_numlock_mask = get_mod_mask_for(XCB_NUM_LOCK, symbols, modmap_reply);

    font = load_font(pattern, true);
    bold_font = load_font(patternbold, true);

    /* Determine character width in the default font. */
    set_font(&font);
    char_width = predict_text_width(i3string_from_utf8("a"));

    /* Open an input window */
    win = xcb_generate_id(conn);
    xcb_create_window(
        conn,
        XCB_COPY_FROM_PARENT,
        win,                                                                /* the window id */
        root,                                                               /* parent == root */
        logical_px(490), logical_px(297), logical_px(300), window_height(), /* dimensions */
        0,                                                                  /* X11 border = 0, we draw our own */
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        XCB_WINDOW_CLASS_COPY_FROM_PARENT, /* copy visual from parent */
        XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
        (uint32_t[]){
            0, /* back pixel: black */
            XCB_EVENT_MASK_EXPOSURE |
                XCB_EVENT_MASK_BUTTON_PRESS});

    /* Map the window (make it visible) */
    xcb_map_window(conn, win);

/* Setup NetWM atoms */
#define xmacro(name)                                                                       \
    do {                                                                                   \
        xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(conn, name##_cookie, NULL); \
        if (!reply)                                                                        \
            errx(EXIT_FAILURE, "Could not get atom " #name "\n");                          \
                                                                                           \
        A_##name = reply->atom;                                                            \
        free(reply);                                                                       \
    } while (0);
#include "atoms.xmacro"
#undef xmacro

    /* Set dock mode */
    xcb_change_property(conn,
                        XCB_PROP_MODE_REPLACE,
                        win,
                        A__NET_WM_WINDOW_TYPE,
                        A_ATOM,
                        32,
                        1,
                        (unsigned char *)&A__NET_WM_WINDOW_TYPE_DIALOG);

    /* Set window title */
    xcb_change_property(conn,
                        XCB_PROP_MODE_REPLACE,
                        win,
                        A__NET_WM_NAME,
                        A_UTF8_STRING,
                        8,
                        strlen("i3: first configuration"),
                        "i3: first configuration");

    /* Create pixmap */
    pixmap = xcb_generate_id(conn);
    pixmap_gc = xcb_generate_id(conn);
    xcb_create_pixmap(conn, root_screen->root_depth, pixmap, win, logical_px(500), logical_px(500));
    xcb_create_gc(conn, pixmap_gc, pixmap, 0, 0);

    /* Grab the keyboard to get all input */
    xcb_flush(conn);

    /* Try (repeatedly, if necessary) to grab the keyboard. We might not
     * get the keyboard at the first attempt because of the keybinding
     * still being active when started via a wm’s keybinding. */
    xcb_grab_keyboard_cookie_t cookie;
    xcb_grab_keyboard_reply_t *reply = NULL;

    int count = 0;
    while ((reply == NULL || reply->status != XCB_GRAB_STATUS_SUCCESS) && (count++ < 500)) {
        cookie = xcb_grab_keyboard(conn, false, win, XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        reply = xcb_grab_keyboard_reply(conn, cookie, NULL);
        usleep(1000);
    }

    if (reply->status != XCB_GRAB_STATUS_SUCCESS) {
        fprintf(stderr, "Could not grab keyboard, status = %d\n", reply->status);
        exit(-1);
    }

    xcb_flush(conn);

    xcb_generic_event_t *event;
    while ((event = xcb_wait_for_event(conn)) != NULL) {
        if (event->response_type == 0) {
            fprintf(stderr, "X11 Error received! sequence %x\n", event->sequence);
            continue;
        }

        /* Strip off the highest bit (set if the event is generated) */
        int type = (event->response_type & 0x7F);

        switch (type) {
            case XCB_KEY_PRESS:
                handle_key_press(NULL, conn, (xcb_key_press_event_t *)event);
                break;

            /* TODO: handle mappingnotify */

            case XCB_BUTTON_PRESS:
                handle_button_press((xcb_button_press_event_t *)event);
                break;

            case XCB_EXPOSE:
                handle_expose();
                break;
        }

        free(event);
    }

    return 0;
}
