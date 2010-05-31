/*
 * vim:ts=4:sw=4:expandtab
 */

#include "all.h"

/* Stores the X11 window ID of the currently focused window */
static xcb_window_t focused_id = XCB_NONE;

/*
 * Describes the X11 state we may modify (map state, position, window stack).
 * There is one entry per container. The state represents the current situation
 * as X11 sees it (with the exception of the order in the state_head CIRCLEQ,
 * which represents the order that will be pushed to X11, while old_state_head
 * represents the current order). It will be updated in x_push_changes().
 *
 */
typedef struct con_state {
    xcb_window_t id;
    bool mapped;
    Rect rect;
    Rect window_rect;

    bool initial;

    CIRCLEQ_ENTRY(con_state) state;
    CIRCLEQ_ENTRY(con_state) old_state;
} con_state;

CIRCLEQ_HEAD(state_head, con_state) state_head =
    CIRCLEQ_HEAD_INITIALIZER(state_head);

CIRCLEQ_HEAD(old_state_head, con_state) old_state_head =
    CIRCLEQ_HEAD_INITIALIZER(old_state_head);

/*
 * Returns the container state for the given frame. This function always
 * returns a container state (otherwise, there is a bug in the code and the
 * container state of a container for which x_con_init() was not called was
 * requested).
 *
 */
static con_state *state_for_frame(xcb_window_t window) {
    con_state *state;
    CIRCLEQ_FOREACH(state, &state_head, state)
        if (state->id == window)
            return state;

    /* TODO: better error handling? */
    ELOG("No state found\n");
    assert(true);
    return NULL;
}

/*
 * Initializes the X11 part for the given container. Called exactly once for
 * every container from con_new().
 *
 */
void x_con_init(Con *con) {
    /* TODO: maybe create the window when rendering first? we could then even
     * get the initial geometry right */

    uint32_t mask = 0;
    uint32_t values[2];

    /* our own frames should not be managed */
    mask |= XCB_CW_OVERRIDE_REDIRECT;
    values[0] = 1;

    /* We want to know when… */
    mask |= XCB_CW_EVENT_MASK;
    values[1] = FRAME_EVENT_MASK;

    Rect dims = { -15, -15, 10, 10 };
    con->frame = create_window(conn, dims, XCB_WINDOW_CLASS_INPUT_OUTPUT, -1, false, mask, values);
    con->gc = xcb_generate_id(conn);
    xcb_create_gc(conn, con->gc, con->frame, 0, 0);

    struct con_state *state = scalloc(sizeof(struct con_state));
    state->id = con->frame;
    state->mapped = false;
    state->initial = true;
    CIRCLEQ_INSERT_HEAD(&state_head, state, state);
    CIRCLEQ_INSERT_HEAD(&old_state_head, state, old_state);
    LOG("adding new state for window id 0x%08x\n", state->id);
}

/*
 * Re-initializes the associated X window state for this container. You have
 * to call this when you assign a client to an empty container to ensure that
 * its state gets updated correctly.
 *
 */
void x_reinit(Con *con) {
    struct con_state *state;

    if ((state = state_for_frame(con->frame)) == NULL) {
        ELOG("window state not found\n");
        return;
    }

    LOG("resetting state %p to initial\n", state);
    state->initial = true;
    memset(&(state->window_rect), 0, sizeof(Rect));
}

void x_con_kill(Con *con) {
    con_state *state;

    xcb_destroy_window(conn, con->frame);
    state = state_for_frame(con->frame);
    CIRCLEQ_REMOVE(&state_head, state, state);
    CIRCLEQ_REMOVE(&old_state_head, state, old_state);
}

/*
 * Returns true if the client supports the given protocol atom (like WM_DELETE_WINDOW)
 *
 */
static bool window_supports_protocol(xcb_window_t window, xcb_atom_t atom) {
    xcb_get_property_cookie_t cookie;
    xcb_get_wm_protocols_reply_t protocols;
    bool result = false;

    cookie = xcb_get_wm_protocols_unchecked(conn, window, atoms[WM_PROTOCOLS]);
    if (xcb_get_wm_protocols_reply(conn, cookie, &protocols, NULL) != 1)
        return false;

    /* Check if the client’s protocols have the requested atom set */
    for (uint32_t i = 0; i < protocols.atoms_len; i++)
        if (protocols.atoms[i] == atom)
            result = true;

    xcb_get_wm_protocols_reply_wipe(&protocols);

    return result;
}

void x_window_kill(xcb_window_t window) {
    /* if this window does not support WM_DELETE_WINDOW, we kill it the hard way */
    if (!window_supports_protocol(window, atoms[WM_DELETE_WINDOW])) {
        LOG("Killing window the hard way\n");
        xcb_kill_client(conn, window);
        return;
    }

    xcb_client_message_event_t ev;

    memset(&ev, 0, sizeof(xcb_client_message_event_t));

    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = window;
    ev.type = atoms[WM_PROTOCOLS];
    ev.format = 32;
    ev.data.data32[0] = atoms[WM_DELETE_WINDOW];
    ev.data.data32[1] = XCB_CURRENT_TIME;

    LOG("Sending WM_DELETE to the client\n");
    xcb_send_event(conn, false, window, XCB_EVENT_MASK_NO_EVENT, (char*)&ev);
    xcb_flush(conn);
}

void x_draw_decoration(Con *con) {
    Con *parent;

    parent = con->parent;

    if (con == focused)
        xcb_change_gc_single(conn, parent->gc, XCB_GC_FOREGROUND, get_colorpixel("#FF0000"));
    else xcb_change_gc_single(conn, parent->gc, XCB_GC_FOREGROUND, get_colorpixel("#0C0C0C"));
    xcb_rectangle_t drect = { con->deco_rect.x, con->deco_rect.y, con->deco_rect.width, con->deco_rect.height };
    xcb_poly_fill_rectangle(conn, parent->frame, parent->gc, 1, &drect);

    if (con->window == NULL)
        return;

    i3Window *win = con->window;

    if (win->name_x == NULL) {
        LOG("not rendering decoration, not yet known\n");
        return;
    }


    LOG("should render text %s onto %p / %s\n", win->name_json, parent, parent->name);

    xcb_change_gc_single(conn, parent->gc, XCB_GC_FOREGROUND, get_colorpixel("#FFFFFF"));
    if (win->uses_net_wm_name)
        xcb_image_text_16(
            conn,
            win->name_len,
            parent->frame,
            parent->gc,
            con->deco_rect.x,
            con->deco_rect.y + 14, /* TODO: hardcoded */
            (xcb_char2b_t*)win->name_x
        );
    else
        xcb_image_text_8(
            conn,
            win->name_len,
            parent->frame,
            parent->gc,
            con->deco_rect.x,
            con->deco_rect.y + 14, /* TODO: hardcoded */
            win->name_x
        );
}

/*
 * This function pushes the properties of each node of the layout tree to
 * X11 if they have changed (like the map state, position of the window, …).
 * It recursively traverses all children of the given node.
 *
 */
static void x_push_node(Con *con) {
    Con *current;
    con_state *state;

    LOG("Pushing changes for node %p / %s\n", con, con->name);
    state = state_for_frame(con->frame);

    /* map/unmap if map state changed, also ensure that the child window
     * is changed if we are mapped *and* in initial state (meaning the
     * container was empty before, but now got a child) */
    if (state->mapped != con->mapped || (con->mapped && state->initial)) {
        if (!con->mapped) {
            LOG("unmapping container\n");
            xcb_unmap_window(conn, con->frame);
        } else {
            if (state->initial && con->window != NULL) {
                LOG("mapping child window\n");
                xcb_map_window(conn, con->window->id);
            }
            LOG("mapping container\n");
            xcb_map_window(conn, con->frame);
        }
        state->mapped = con->mapped;
    }

    bool fake_notify = false;
    /* set new position if rect changed */
    if (memcmp(&(state->rect), &(con->rect), sizeof(Rect)) != 0) {
        LOG("setting rect (%d, %d, %d, %d)\n", con->rect.x, con->rect.y, con->rect.width, con->rect.height);
        xcb_set_window_rect(conn, con->frame, con->rect);
        memcpy(&(state->rect), &(con->rect), sizeof(Rect));
        fake_notify = true;
    }

    /* dito, but for child windows */
    if (memcmp(&(state->window_rect), &(con->window_rect), sizeof(Rect)) != 0) {
        LOG("setting window rect (%d, %d, %d, %d)\n",
            con->window_rect.x, con->window_rect.y, con->window_rect.width, con->window_rect.height);
        xcb_set_window_rect(conn, con->window->id, con->window_rect);
        memcpy(&(state->rect), &(con->rect), sizeof(Rect));
        fake_notify = true;
    }

    if (fake_notify) {
        LOG("Sending fake configure notify\n");
        fake_absolute_configure_notify(con);
    }

    /* handle all children and floating windows of this node */
    TAILQ_FOREACH(current, &(con->nodes_head), nodes)
        x_push_node(current);

    TAILQ_FOREACH(current, &(con->floating_head), floating_windows)
        x_push_node(current);

    if (con->type != CT_ROOT && con->type != CT_OUTPUT)
        x_draw_decoration(con);
}

/*
 * Pushes all changes (state of each node, see x_push_node() and the window
 * stack) to X11.
 *
 */
void x_push_changes(Con *con) {
    con_state *state;

    LOG("\n\n PUSHING CHANGES\n\n");
    x_push_node(con);

    LOG("-- PUSHING WINDOW STACK --\n");
    /* X11 correctly represents the stack if we push it from bottom to top */
    CIRCLEQ_FOREACH_REVERSE(state, &state_head, state) {
        LOG("stack: 0x%08x\n", state->id);
        con_state *prev = CIRCLEQ_PREV(state, state);
        con_state *old_prev = CIRCLEQ_PREV(state, old_state);
        if ((state->initial || prev != old_prev) && prev != CIRCLEQ_END(&state_head)) {
            state->initial = false;
            LOG("Stacking 0x%08x above 0x%08x\n", prev->id, state->id);
            uint32_t mask = 0;
            mask |= XCB_CONFIG_WINDOW_SIBLING;
            mask |= XCB_CONFIG_WINDOW_STACK_MODE;
            uint32_t values[] = {state->id, XCB_STACK_MODE_ABOVE};

            xcb_configure_window(conn, prev->id, mask, values);
        }
    }

    xcb_window_t to_focus = focused->frame;
    if (focused->window != NULL)
        to_focus = focused->window->id;

    if (focused_id != to_focus) {
        LOG("Updating focus (focused: %p / %s)\n", focused, focused->name);
        xcb_set_input_focus(conn, XCB_INPUT_FOCUS_POINTER_ROOT, to_focus, XCB_CURRENT_TIME);
    }

    xcb_flush(conn);
    LOG("\n\n ENDING CHANGES\n\n");

    /* save the current stack as old stack */
    CIRCLEQ_FOREACH(state, &state_head, state) {
        CIRCLEQ_REMOVE(&old_state_head, state, old_state);
        CIRCLEQ_INSERT_TAIL(&old_state_head, state, old_state);
    }
    CIRCLEQ_FOREACH(state, &old_state_head, old_state) {
        LOG("old stack: 0x%08x\n", state->id);
    }
}

/*
 * Raises the specified container in the internal stack of X windows. The
 * next call to x_push_changes() will make the change visible in X11.
 *
 */
void x_raise_con(Con *con) {
    con_state *state;
    LOG("raising in new stack: %p / %s\n", con, con->name);
    state = state_for_frame(con->frame);

    LOG("found state entry, moving to top\n");
    CIRCLEQ_REMOVE(&state_head, state, state);
    CIRCLEQ_INSERT_HEAD(&state_head, state, state);
}
