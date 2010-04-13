/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2010 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "all.h"

void window_update_class(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("empty property, not updating\n");
        return;
    }

    /* We cannot use asprintf here since this property contains two
     * null-terminated strings (for compatibility reasons). Instead, we
     * use strdup() on both strings */
    char *new_class = xcb_get_property_value(prop);

    FREE(win->class_instance);
    FREE(win->class_class);

    win->class_instance = strdup(new_class);
    if ((strlen(new_class) + 1) < xcb_get_property_value_length(prop))
            win->class_class = strdup(new_class + strlen(new_class) + 1);
    else win->class_class = NULL;
    LOG("WM_CLASS changed to %s (instance), %s (class)\n",
        win->class_instance, win->class_class);
}

void window_update_name(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("_NET_WM_NAME not specified, not changing\n");
        return 1;
    }

    /* Save the old pointer to make the update atomic */
    int new_len;
    asprintf(&win->name_utf8, "%.*s", xcb_get_property_value_length(prop), (char*)xcb_get_property_value(prop));
    /* Convert it to UCS-2 here for not having to convert it later every time we want to pass it to X */
    win->name_ucs2 = convert_utf8_to_ucs2(win->name_utf8, &win->name_len);
    LOG("_NET_WM_NAME changed to \"%s\"\n", win->name_utf8);

    win->uses_net_wm_name = true;
}
