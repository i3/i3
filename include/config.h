#ifndef _CONFIG_H
#define _CONFIG_H

typedef struct Config Config;
extern Config config;

struct Colortriple {
	char border[8];
	char background[8];
	char text[8];
};

struct Config {
	const char *terminal;
	const char *font;

	/* Color codes are stored here */
	struct config_client {
		struct Colortriple focused;
		struct Colortriple focused_inactive;
		struct Colortriple unfocused;
	} client;
	struct config_bar {
		struct Colortriple focused;
		struct Colortriple unfocused;
	} bar;
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
