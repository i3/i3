#ifndef CONFIG_H_
#define CONFIG_H_

#include "common.h"

typedef struct config_t {
    int          hide_on_modifier;
    int          verbose;
    xcb_colors_t *colors;
} config_t;

config_t config;

#endif
