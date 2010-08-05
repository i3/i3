#ifndef XCB_H_
#define XCB_H_

int font_height;

void init_xcb();
void clean_xcb();
void get_atoms();
void destroy_windows();
void create_windows();
void draw_bars();
int get_string_width(char *string);

#endif
