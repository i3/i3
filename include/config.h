#ifndef _CONFIG_H
#define _CONFIG_H

typedef struct Config Config;
extern Config config;

struct Config {
	const char *terminal;
	const char *font;

	/* Color codes are stored here */
	char *client_focused_background_active;
	char *client_focused_background_inactive;
	char *client_focused_text;
	char *client_focused_border;
	char *client_unfocused_background;
	char *client_unfocused_text;
	char *client_unfocused_border;
	char *bar_focused_background;
	char *bar_focused_text;
	char *bar_focused_border;
	char *bar_unfocused_background;
	char *bar_unfocused_text;
	char *bar_unfocused_border;
};

/**
 * Reads the configuration from ~/.i3/config or /etc/i3/config if not found.
 *
 * If you specify override_configpath, only this path is used to look for a
 * configuration file.
 *
 */
void load_configuration(const char *override_configfile);

#endif
