#include <xcb/xcb.h>

#ifndef _COMMANDS_H
#define _COMMANDS_H

void parse_command(xcb_connection_t *conn, const char *command);

#endif
