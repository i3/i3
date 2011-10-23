#ifndef _I3_INPUT
#define _I3_INPUT

#include <err.h>

#define die(...) errx(EXIT_FAILURE, __VA_ARGS__);
#define FREE(pointer) do { \
        if (pointer != NULL) { \
                free(pointer); \
                pointer = NULL; \
        } \
} \
while (0)

extern xcb_window_t root;

char *convert_ucs_to_utf8(char *input);
char *convert_utf8_to_ucs2(char *input, int *real_strlen);

#endif
