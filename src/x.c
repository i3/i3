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

    /* For reparenting, we have a flag (need_reparent) and the X ID of the old
     * frame this window was in. The latter is necessary because we need to
     * ignore UnmapNotify events (by changing the window event mask). */
    bool need_reparent;
    xcb_window_t old_frame;

    Rect rect;
    Rect window_rect;

    bool initial;

    char *name;

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
    assert(false);
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

/*
 * Reparents the child window of the given container (necessary for sticky
 * containers). The reparenting happens in the next call of x_push_changes().
 *
 */
void x_reparent_child(Con *con, Con *old) {
    struct con_state *state;
    if ((state = state_for_frame(con->frame)) == NULL) {
        ELOG("window state for con not found\n");
        return;
    }

    state->need_reparent = true;
    state->old_frame = old->frame;
}

/*
 * Moves a child window from Container src to Container dest.
 *
 */
void x_move_win(Con *src, Con *dest) {
    struct con_state *state_src, *state_dest;

    if ((state_src = state_for_frame(src->frame)) == NULL) {
        ELOG("window state for src not found\n");
        return;
    }

    if ((state_dest = state_for_frame(dest->frame)) == NULL) {
        ELOG("window state for dest not found\n");
        return;
    }

    Rect zero;
    memset(&zero, 0, sizeof(Rect));
    if (memcmp(&(state_dest->window_rect), &(zero), sizeof(Rect)) == 0) {
        memcpy(&(state_dest->window_rect), &(state_src->window_rect), sizeof(Rect));
        LOG("COPYING RECT\n");
    }
}

/*
 * Kills the window decoration associated with the given container.
 *
 */
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

/*
 * Kills the given X11 window using WM_DELETE_WINDOW (if supported).
 *
 */
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

/*
 * Draws the decoration of the given container onto its parent.
 *
 */
void x_draw_decoration(Con *con) {
    /* This code needs to run for:
     *  • leaf containers
     *  • non-leaf containers which are in a stacking container
     *
     * It does not need to run for:
     *  • floating containers (they don’t have a decoration)
     */
    if ((!con_is_leaf(con) && con->parent->layout != L_STACKED) ||
        con->type == CT_FLOATING_CON)
        return;
    DLOG("decoration should be rendered for con %p\n", con);

    /* 1: find out which colors to use */
    struct Colortriple *color;
    if (con->urgent)
        color = &config.client.urgent;
    else if (con == focused)
        color = &config.client.focused;
    else if (con == TAILQ_FIRST(&(con->parent->focus_head)))
        color = &config.client.focused_inactive;
    else
        color = &config.client.unfocused;

    Con *parent = con->parent;
    int border_style = con_border_style(con);

    /* 2: draw a rectangle in border color around the client */
    if (border_style != BS_NONE && con_is_leaf(con)) {
        Rect br = con_border_style_rect(con);
        Rect *r = &(con->rect);
#if 0
        DLOG("con->rect spans %d x %d\n", con->rect.width, con->rect.height);
        DLOG("border_rect spans (%d, %d) with %d x %d\n", border_rect.x, border_rect.y, border_rect.width, border_rect.height);
        DLOG("window_rect spans (%d, %d) with %d x %d\n", con->window_rect.x, con->window_rect.y, con->window_rect.width, con->window_rect.height);
#endif

        /* These rectangles represents the border around the child window
         * (left, bottom and right part). We don’t just fill the whole
         * rectangle because some childs are not freely resizable and we want
         * their background color to "shine through". */
        xcb_change_gc_single(conn, con->gc, XCB_GC_FOREGROUND, color->background);
        xcb_rectangle_t borders[] = {
            { 0, 0, br.x, r->height },
            { 0, r->height + br.height + br.y, r->width, r->height },
            { r->width + br.width + br.x, 0, r->width, r->height }
        };
        xcb_poly_fill_rectangle(conn, con->frame, con->gc, 3, borders);
        /* 1pixel border needs an additional line at the top */
        if (border_style == BS_1PIXEL) {
            xcb_rectangle_t topline = { br.x, 0, con->rect.width + br.width + br.x, br.y };
            xcb_poly_fill_rectangle(conn, con->frame, con->gc, 1, &topline);
        }
    }

    /* if this is a borderless/1pixel window, we don’t * need to render the
     * decoration. */
    if (border_style != BS_NORMAL) {
        DLOG("border style not BS_NORMAL, aborting rendering of decoration\n");
        return;
    }

    /* 3: paint the bar */
    xcb_change_gc_single(conn, parent->gc, XCB_GC_FOREGROUND, color->background);
    xcb_rectangle_t drect = { con->deco_rect.x, con->deco_rect.y, con->deco_rect.width, con->deco_rect.height };
    xcb_poly_fill_rectangle(conn, parent->frame, parent->gc, 1, &drect);

    /* 4: draw the two lines in border color */
    xcb_draw_line(conn, parent->frame, parent->gc, color->border,
            con->deco_rect.x, /* x */
            con->deco_rect.y, /* y */
            con->deco_rect.x + con->deco_rect.width, /* to_x */
            con->deco_rect.y); /* to_y */
    xcb_draw_line(conn, parent->frame, parent->gc, color->border,
            con->deco_rect.x, /* x */
            con->deco_rect.y + con->deco_rect.height - 1, /* y */
            con->deco_rect.x + con->deco_rect.width, /* to_x */
            con->deco_rect.y + con->deco_rect.height - 1); /* to_y */

    /* 5: draw the title */
    i3Font *font = load_font(conn, config.font);
    uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
    uint32_t values[] = { color->text, color->background, font->id };
    xcb_change_gc(conn, parent->gc, mask, values);
    int text_offset_y = font->height + (con->deco_rect.height - font->height) / 2 - 1;

    struct Window *win = con->window;
    if (win == NULL || win->name_x == NULL) {
        /* this is a non-leaf container, we need to make up a good description */
        // TODO: use a good description instead of just "another container"
        xcb_image_text_8(
            conn,
            strlen("another container"),
            parent->frame,
            parent->gc,
            con->deco_rect.x + 2,
            con->deco_rect.y + text_offset_y,
            "another container"
        );
        return;
    }

    int indent_level = 0,
        indent_mult = 0;
    Con *il_parent = con->parent;
    if (il_parent->type != L_STACKED) {
        while (1) {
            DLOG("il_parent = %p, layout = %d\n", il_parent, il_parent->layout);
            if (il_parent->layout == L_STACKED)
                indent_level++;
            if (il_parent->type == CT_WORKSPACE)
                break;
            il_parent = il_parent->parent;
            indent_mult++;
        }
    }
    DLOG("indent_level = %d, indent_mult = %d\n", indent_level, indent_mult);
    int indent_px = (indent_level * 5) * indent_mult;

    if (win->uses_net_wm_name)
        xcb_image_text_16(
            conn,
            win->name_len,
            parent->frame,
            parent->gc,
            con->deco_rect.x + 2 + indent_px,
            con->deco_rect.y + text_offset_y,
            (xcb_char2b_t*)win->name_x
        );
    else
        xcb_image_text_8(
            conn,
            win->name_len,
            parent->frame,
            parent->gc,
            con->deco_rect.x + 2 + indent_px,
            con->deco_rect.y + text_offset_y,
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
    Rect rect = con->rect;

    LOG("Pushing changes for node %p / %s\n", con, con->name);
    state = state_for_frame(con->frame);

    if (state->name != NULL) {
        DLOG("pushing name %s for con %p\n", state->name, con);

        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, con->frame,
                            WM_NAME, STRING, 8, strlen(state->name), state->name);
        FREE(state->name);
    }

    if (con->window == NULL) {
        /* Calculate the height of all window decorations which will be drawn on to
         * this frame. */
        uint32_t max_y = 0, max_height = 0;
        TAILQ_FOREACH(current, &(con->nodes_head), nodes) {
            DLOG("Child's decoration is %d x %d, from (%d, %d)\n",
                    current->deco_rect.width, current->deco_rect.height,
                    current->deco_rect.x, current->deco_rect.y);
            Rect *dr = &(current->deco_rect);
            if (dr->y >= max_y && dr->height >= max_height) {
                max_y = dr->y;
                max_height = dr->height;
            }
        }
        DLOG("bottom of decorations is %d\n", max_y + max_height);
        rect.height = max_y + max_height;
        if (rect.height == 0) {
            DLOG("Unmapping container because it does not contain anything atm.\n");
            con->mapped = false;
        }
    }

    /* reparent the child window (when the window was moved due to a sticky
     * container) */
    if (state->need_reparent && con->window != NULL) {
        LOG("Reparenting child window\n");

        /* Temporarily set the event masks to XCB_NONE so that we won’t get
         * UnmapNotify events (otherwise the handler would close the container).
         * These events are generated automatically when reparenting. */
        uint32_t values[] = { XCB_NONE };
        xcb_change_window_attributes(conn, state->old_frame, XCB_CW_EVENT_MASK, values);
        xcb_change_window_attributes(conn, con->window->id, XCB_CW_EVENT_MASK, values);

        xcb_reparent_window(conn, con->window->id, con->frame, 0, 0);

        values[0] = FRAME_EVENT_MASK;
        xcb_change_window_attributes(conn, state->old_frame, XCB_CW_EVENT_MASK, values);
        values[0] = CHILD_EVENT_MASK;
        xcb_change_window_attributes(conn, con->window->id, XCB_CW_EVENT_MASK, values);

        state->old_frame = XCB_NONE;
        state->need_reparent = false;

        con->ignore_unmap++;
        DLOG("ignore_unmap for reparenting of con %p (win 0x%08x) is now %d\n",
                con, con->window->id, con->ignore_unmap);
    }

    /* map/unmap if map state changed, also ensure that the child window
     * is changed if we are mapped *and* in initial state (meaning the
     * container was empty before, but now got a child) */
    if (state->mapped != con->mapped || (con->mapped && state->initial)) {
        if (!con->mapped) {
            xcb_void_cookie_t cookie;
            if (con->window != NULL) {
                /* Set WM_STATE_WITHDRAWN, it seems like Java apps need it */
                long data[] = { XCB_WM_STATE_WITHDRAWN, XCB_NONE };
                xcb_change_property(conn, XCB_PROP_MODE_REPLACE, con->window->id,
                                    atoms[WM_STATE], atoms[WM_STATE], 32, 2, data);
            }

            cookie = xcb_unmap_window(conn, con->frame);
            LOG("unmapping container (serial %d)\n", cookie.sequence);
            /* we need to increase ignore_unmap for this container (if it contains a window) and for every window "under" this one which contains a window */
            if (con->window != NULL) {
                con->ignore_unmap++;
                DLOG("ignore_unmap for con %p (frame 0x%08x) now %d\n", con, con->frame, con->ignore_unmap);
            }
            /* Ignore enter_notifies which are generated when unmapping */
            add_ignore_event(cookie.sequence);
        } else {
            xcb_void_cookie_t cookie;

            if (con->window != NULL) {
                /* Set WM_STATE_NORMAL because GTK applications don’t want to
                 * drag & drop if we don’t. Also, xprop(1) needs it. */
                long data[] = { XCB_WM_STATE_NORMAL, XCB_NONE };
                xcb_change_property(conn, XCB_PROP_MODE_REPLACE, con->window->id,
                                    atoms[WM_STATE], atoms[WM_STATE], 32, 2, data);
            }

            if (state->initial && con->window != NULL) {
                cookie = xcb_map_window(conn, con->window->id);
                LOG("mapping child window (serial %d)\n", cookie.sequence);
                /* Ignore enter_notifies which are generated when mapping */
                add_ignore_event(cookie.sequence);
            }

            cookie = xcb_map_window(conn, con->frame);
            LOG("mapping container (serial %d)\n", cookie.sequence);
            /* Ignore enter_notifies which are generated when mapping */
            add_ignore_event(cookie.sequence);
        }
        state->mapped = con->mapped;
    }

    bool fake_notify = false;
    /* set new position if rect changed */
    if (memcmp(&(state->rect), &rect, sizeof(Rect)) != 0) {
        LOG("setting rect (%d, %d, %d, %d)\n", rect.x, rect.y, rect.width, rect.height);
        xcb_set_window_rect(conn, con->frame, rect);
        memcpy(&(state->rect), &rect, sizeof(Rect));
        fake_notify = true;
    }

    /* dito, but for child windows */
    if (con->window != NULL &&
        memcmp(&(state->window_rect), &(con->window_rect), sizeof(Rect)) != 0) {
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
    bool order_changed = false;
    /* X11 correctly represents the stack if we push it from bottom to top */
    CIRCLEQ_FOREACH_REVERSE(state, &state_head, state) {
        LOG("stack: 0x%08x\n", state->id);
        con_state *prev = CIRCLEQ_PREV(state, state);
        con_state *old_prev = CIRCLEQ_PREV(state, old_state);
        if (prev != old_prev)
            order_changed = true;
        if ((state->initial || order_changed) && prev != CIRCLEQ_END(&state_head)) {
            LOG("Stacking 0x%08x above 0x%08x\n", prev->id, state->id);
            uint32_t mask = 0;
            mask |= XCB_CONFIG_WINDOW_SIBLING;
            mask |= XCB_CONFIG_WINDOW_STACK_MODE;
            uint32_t values[] = {state->id, XCB_STACK_MODE_ABOVE};

            xcb_configure_window(conn, prev->id, mask, values);
        }
        state->initial = false;
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

/*
 * Sets the WM_NAME property (so, no UTF8, but used only for debugging anyways)
 * of the given name. Used for properly tagging the windows for easily spotting
 * i3 windows in xwininfo -root -all.
 *
 */
void x_set_name(Con *con, const char *name) {
    struct con_state *state;

    if ((state = state_for_frame(con->frame)) == NULL) {
        ELOG("window state not found\n");
        return;
    }

    FREE(state->name);
    state->name = sstrdup(name);
}
