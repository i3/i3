#include <xcb/xcb.h>

#ifndef _LAYOUT_H
#define _LAYOUT_H

void decorate_window(xcb_connection_t *conn, Client *client);
void render_layout(xcb_connection_t *conn);

#endif
