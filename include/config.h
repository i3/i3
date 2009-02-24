#ifndef _CONFIG_H
#define _CONFIG_H

typedef struct Config Config;
extern Config config;

struct Config {
	const char *terminal;
	const char *font;
};

void load_configuration(const char *configfile);

#endif
