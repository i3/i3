/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010-2011 Axel Wagner and contributors (see also: LICENSE)
 *
 * config.c: Parses the configuration (received from i3).
 *
 */
#ifndef CONFIG_H_
#define CONFIG_H_

#include "common.h"

typedef enum {
    POS_NONE = 0,
    POS_TOP,
    POS_BOT
} position_t;

typedef struct config_t {
    int          hide_on_modifier;
    int          modifier;
    position_t   position;
    int          verbose;
    struct xcb_color_strings_t colors;
    int          disable_ws;
    char         *bar_id;
    char         *command;
    char         *fontname;
    char         *tray_output;
    int          num_outputs;
    char         **outputs;
} config_t;

config_t config;

/**
 * Start parsing the received bar configuration json-string
 *
 */
void parse_config_json(char *json);

/**
 * free()s the color strings as soon as they are not needed anymore.
 *
 */
void free_colors(struct xcb_color_strings_t *colors);

#endif
