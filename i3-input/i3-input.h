#pragma once

#include <err.h>

#define die(...) errx(EXIT_FAILURE, __VA_ARGS__);
#define FREE(pointer)          \
    do {                       \
        if (pointer != NULL) { \
            free(pointer);     \
            pointer = NULL;    \
        }                      \
    } while (0)

extern xcb_window_t root;
