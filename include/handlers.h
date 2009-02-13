#ifndef _HANDLERS_H
#define _HANDLERS_H

int handle_key_release(void *ignored, xcb_connection_t *conn, xcb_key_release_event_t *event);
int handle_key_press(void *ignored, xcb_connection_t *conn, xcb_key_press_event_t *event);
int handle_enter_notify(void *ignored, xcb_connection_t *conn, xcb_enter_notify_event_t *event);
int handle_button_press(void *ignored, xcb_connection_t *conn, xcb_button_press_event_t *event);
int handle_map_notify_event(void *prophs, xcb_connection_t *conn, xcb_map_notify_event_t *event);
int handle_windowname_change(void *data, xcb_connection_t *conn, uint8_t state,
				xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *prop);
int handle_expose_event(void *data, xcb_connection_t *conn, xcb_expose_event_t *event);





#endif
