#undef I3__FILE__
#define I3__FILE__ "key_press.c"
/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * key_press.c: key press handler
 *
 */
#include "all.h"

/*
 * There was a KeyPress or KeyRelease (both events have the same fields). We
 * compare this key code with our bindings table and pass the bound action to
 * parse_command().
 *
 */
void handle_key_press(xcb_key_press_event_t *event) {
    bool key_release = (event->response_type == XCB_KEY_RELEASE);

    last_timestamp = event->time;

    DLOG("%s %d, state raw = %d\n", (key_release ? "KeyRelease" : "KeyPress"), event->detail, event->state);

    Binding *bind = get_binding_from_xcb_event((xcb_generic_event_t *)event);

    /* if we couldn't find a binding, we are done */
    if (bind == NULL)
        return;

    CommandResult *result = run_binding(bind, NULL);

    if (result->needs_tree_render)
        tree_render();

    command_result_free(result);
}
