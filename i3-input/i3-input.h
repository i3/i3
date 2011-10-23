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
uint32_t get_mod_mask(xcb_connection_t *conn, uint32_t keycode);
xcb_window_t open_input_window(xcb_connection_t *conn, uint32_t width, uint32_t height);
int get_font_id(xcb_connection_t *conn, char *pattern, int *font_height);
void xcb_change_gc_single(xcb_connection_t *conn, xcb_gcontext_t gc, uint32_t mask, uint32_t value);

#endif
