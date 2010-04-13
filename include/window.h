#ifndef _WINDOW_H
#define _WINDOW_H

void window_update_class(i3Window *win, xcb_get_property_reply_t *prop);
void window_update_name(i3Window *win, xcb_get_property_reply_t *prop);
void window_update_name_legacy(i3Window *win, xcb_get_property_reply_t *prop);

#endif
