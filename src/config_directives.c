#undef I3__FILE__
#define I3__FILE__ "config_directives.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2012 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * config_directives.c: all command functions (see config_parser.c)
 *
 */
#include <float.h>
#include <stdarg.h>

#include "all.h"

// Macros to make the YAJL API a bit easier to use.
#define y(x, ...) yajl_gen_ ## x (cmd_output->json_gen, ##__VA_ARGS__)
#define ystr(str) yajl_gen_string(cmd_output->json_gen, (unsigned char*)str, strlen(str))
#define ysuccess(success) do { \
    y(map_open); \
    ystr("success"); \
    y(bool, success); \
    y(map_close); \
} while (0)

static char *font_pattern;

void cfg_font(I3_CFG, const char *font) {
	config.font = load_font(font, true);
	set_font(&config.font);

	/* Save the font pattern for using it as bar font later on */
	FREE(font_pattern);
	font_pattern = sstrdup(font);
}

void cfg_mode_binding(I3_CFG, const char *bindtype, const char *modifiers, const char *key, const char *command) {
	printf("cfg_mode_binding: got bindtype\n");
}

void cfg_enter_mode(I3_CFG, const char *mode) {
	// TODO: error handling: if mode == '{', the mode name is missing
	printf("mode name: %s\n", mode);
}

void cfg_exec(I3_CFG, const char *exectype, const char *no_startup_id, const char *command) {
	struct Autostart *new = smalloc(sizeof(struct Autostart));
	new->command = sstrdup(command);
	new->no_startup_id = (no_startup_id != NULL);
	if (strcmp(exectype, "exec") == 0) {
		TAILQ_INSERT_TAIL(&autostarts, new, autostarts);
	} else {
		TAILQ_INSERT_TAIL(&autostarts_always, new, autostarts_always);
	}
}
