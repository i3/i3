/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3bar - an xcb-based status- and ws-bar for i3
 * © 2010 Axel Wagner and contributors (see also: LICENSE)
 *
 * config.c: Parses the configuration (received from i3).
 *
 */
#pragma once

#include <config.h>

#include "common.h"

typedef enum {
    POS_NONE = 0,
    POS_TOP,
    POS_BOT
} position_t;

/* Bar display mode (hide unless modifier is pressed or show in dock mode or always hide in invisible mode) */
typedef enum { M_DOCK = 0,
               M_HIDE = 1,
               M_INVISIBLE = 2 } bar_display_mode_t;

typedef struct binding_t {
    int input_code;
    char *command;

    TAILQ_ENTRY(binding_t)
    bindings;
} binding_t;

typedef struct tray_output_t {
    char *output;

    TAILQ_ENTRY(tray_output_t)
    tray_outputs;
} tray_output_t;

typedef struct config_t {
    int modifier;

    TAILQ_HEAD(bindings_head, binding_t)
    bindings;

    position_t position;
    int verbose;
    struct xcb_color_strings_t colors;
    bool disable_binding_mode_indicator;
    bool disable_ws;
    bool strip_ws_numbers;
    char *bar_id;
    char *command;
    char *fontname;
    i3String *separator_symbol;

    TAILQ_HEAD(tray_outputs_head, tray_output_t)
    tray_outputs;

    int tray_padding;
    int num_outputs;
    char **outputs;

    bar_display_mode_t hide_on_modifier;

    /* The current hidden_state of the bar, which indicates whether it is hidden or shown */
    enum { S_HIDE = 0,
           S_SHOW = 1 } hidden_state;
} config_t;

config_t config;

/**
 * Start parsing the received bar configuration JSON string
 *
 */
void parse_config_json(char *json);

/**
 * free()s the color strings as soon as they are not needed anymore.
 *
 */
void free_colors(struct xcb_color_strings_t *colors);
