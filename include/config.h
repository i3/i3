#ifndef _CONFIG_H
#define _CONFIG_H

typedef struct Config Config;
extern Config config;

struct Config {
	const char *terminal;
	const char *font;
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
