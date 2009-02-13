#ifndef _UTIL_H
#define _UTIL_H

void start_application(const char *path, const char *args);
void check_error(xcb_connection_t *connection, xcb_void_cookie_t cookie, char *err_message);

#endif
