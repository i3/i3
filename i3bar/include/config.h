#ifndef CONFIG_H_
#define CONFIG_H_

#include "common.h"

typedef enum {
    DOCKPOS_NONE = 0,
    DOCKPOS_TOP,
    DOCKPOS_BOT
} dockpos_t;

typedef struct config_t {
    int          hide_on_modifier;
    dockpos_t    dockpos;
    int          verbose;
    xcb_colors_t *colors;
} config_t;

config_t config;

#endif
