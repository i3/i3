/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 * window.c: Updates window attributes (X11 hints/properties).
 *
 */
#include "all.h"

#include <math.h>

/*
 * Frees an i3Window and all its members.
 *
 */
void window_free(i3Window *win) {
    FREE(win->class_class);
    FREE(win->class_instance);
    FREE(win->role);
    FREE(win->machine);
    i3string_free(win->name);
    cairo_surface_destroy(win->icon);
    FREE(win->ran_assignments);
    FREE(win);
}

/*
 * Updates the WM_CLASS (consisting of the class and instance) for the
 * given window.
 *
 */
void window_update_class(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("WM_CLASS not set.\n");
        FREE(prop);
        return;
    }

    /* We cannot use asprintf here since this property contains two
     * null-terminated strings (for compatibility reasons). Instead, we
     * use strdup() on both strings */
    const size_t prop_length = xcb_get_property_value_length(prop);
    char *new_class = xcb_get_property_value(prop);
    const size_t class_class_index = strnlen(new_class, prop_length) + 1;

    FREE(win->class_instance);
    FREE(win->class_class);

    win->class_instance = sstrndup(new_class, prop_length);
    if (class_class_index < prop_length)
        win->class_class = sstrndup(new_class + class_class_index, prop_length - class_class_index);
    else
        win->class_class = NULL;
    LOG("WM_CLASS changed to %s (instance), %s (class)\n",
        win->class_instance, win->class_class);

    free(prop);
}

/*
 * Updates the name by using _NET_WM_NAME (encoded in UTF-8) for the given
 * window. Further updates using window_update_name_legacy will be ignored.
 *
 */
void window_update_name(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("_NET_WM_NAME not specified, not changing\n");
        FREE(prop);
        return;
    }

    i3string_free(win->name);

    /* Truncate the name at the first zero byte. See #3515. */
    const int len = xcb_get_property_value_length(prop);
    char *name = sstrndup(xcb_get_property_value(prop), len);
    win->name = i3string_from_utf8(name);
    free(name);

    Con *con = con_by_window_id(win->id);
    if (con != NULL && con->title_format != NULL) {
        i3String *name = con_parse_title_format(con);
        ewmh_update_visible_name(win->id, i3string_as_utf8(name));
        I3STRING_FREE(name);
    }
    win->name_x_changed = true;
    LOG("_NET_WM_NAME changed to \"%s\"\n", i3string_as_utf8(win->name));

    win->uses_net_wm_name = true;

    free(prop);
}

/*
 * Updates the name by using WM_NAME (encoded in COMPOUND_TEXT). We do not
 * touch what the client sends us but pass it to xcb_image_text_8. To get
 * proper unicode rendering, the application has to use _NET_WM_NAME (see
 * window_update_name()).
 *
 */
void window_update_name_legacy(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("WM_NAME not set (_NET_WM_NAME is what you want anyways).\n");
        FREE(prop);
        return;
    }

    /* ignore update when the window is known to already have a UTF-8 name */
    if (win->uses_net_wm_name) {
        free(prop);
        return;
    }

    i3string_free(win->name);
    const int len = xcb_get_property_value_length(prop);
    char *name = sstrndup(xcb_get_property_value(prop), len);
    win->name = i3string_from_utf8(name);
    free(name);

    Con *con = con_by_window_id(win->id);
    if (con != NULL && con->title_format != NULL) {
        i3String *name = con_parse_title_format(con);
        ewmh_update_visible_name(win->id, i3string_as_utf8(name));
        I3STRING_FREE(name);
    }

    LOG("WM_NAME changed to \"%s\"\n", i3string_as_utf8(win->name));
    LOG("Using legacy window title. Note that in order to get Unicode window "
        "titles in i3, the application has to set _NET_WM_NAME (UTF-8)\n");

    win->name_x_changed = true;

    free(prop);
}

/*
 * Updates the CLIENT_LEADER (logical parent window).
 *
 */
void window_update_leader(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("CLIENT_LEADER not set on window 0x%08x.\n", win->id);
        win->leader = XCB_NONE;
        FREE(prop);
        return;
    }

    xcb_window_t *leader = xcb_get_property_value(prop);
    if (leader == NULL) {
        free(prop);
        return;
    }

    DLOG("Client leader changed to %08x\n", *leader);

    win->leader = *leader;

    free(prop);
}

/*
 * Updates the TRANSIENT_FOR (logical parent window).
 *
 */
void window_update_transient_for(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("TRANSIENT_FOR not set on window 0x%08x.\n", win->id);
        win->transient_for = XCB_NONE;
        FREE(prop);
        return;
    }

    xcb_window_t transient_for;
    if (!xcb_icccm_get_wm_transient_for_from_reply(&transient_for, prop)) {
        free(prop);
        return;
    }

    DLOG("Transient for changed to 0x%08x (window 0x%08x)\n", transient_for, win->id);

    win->transient_for = transient_for;

    free(prop);
}

/*
 * Updates the _NET_WM_STRUT_PARTIAL (reserved pixels at the screen edges)
 *
 */
void window_update_strut_partial(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("_NET_WM_STRUT_PARTIAL not set.\n");
        FREE(prop);
        return;
    }

    uint32_t *strut;
    if (!(strut = xcb_get_property_value(prop))) {
        free(prop);
        return;
    }

    DLOG("Reserved pixels changed to: left = %d, right = %d, top = %d, bottom = %d\n",
         strut[0], strut[1], strut[2], strut[3]);

    win->reserved = (struct reservedpx){strut[0], strut[1], strut[2], strut[3]};

    free(prop);
}

/*
 * Updates the WM_WINDOW_ROLE
 *
 */
void window_update_role(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("WM_WINDOW_ROLE not set.\n");
        FREE(prop);
        return;
    }

    char *new_role;
    sasprintf(&new_role, "%.*s", xcb_get_property_value_length(prop),
              (char *)xcb_get_property_value(prop));
    FREE(win->role);
    win->role = new_role;
    LOG("WM_WINDOW_ROLE changed to \"%s\"\n", win->role);

    free(prop);
}

/*
 * Updates the _NET_WM_WINDOW_TYPE property.
 *
 */
void window_update_type(i3Window *window, xcb_get_property_reply_t *reply) {
    xcb_atom_t new_type = xcb_get_preferred_window_type(reply);
    free(reply);
    if (new_type == XCB_NONE) {
        DLOG("cannot read _NET_WM_WINDOW_TYPE from window.\n");
        return;
    }

    window->window_type = new_type;
    LOG("_NET_WM_WINDOW_TYPE changed to %i.\n", window->window_type);

    run_assignments(window);
}

/*
 * Updates the WM_NORMAL_HINTS
 *
 */
bool window_update_normal_hints(i3Window *win, xcb_get_property_reply_t *reply, xcb_get_geometry_reply_t *geom) {
    bool changed = false;
    xcb_size_hints_t size_hints;

    /* If the hints were already in this event, use them, if not, request them */
    bool success;
    if (reply != NULL) {
        success = xcb_icccm_get_wm_size_hints_from_reply(&size_hints, reply);
    } else {
        success = xcb_icccm_get_wm_normal_hints_reply(conn, xcb_icccm_get_wm_normal_hints_unchecked(conn, win->id), &size_hints, NULL);
    }
    if (!success) {
        DLOG("Could not get WM_NORMAL_HINTS\n");
        return false;
    }

#define ASSIGN_IF_CHANGED(original, new) \
    do {                                 \
        if (original != new) {           \
            original = new;              \
            changed = true;              \
        }                                \
    } while (0)

    if ((size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE)) {
        DLOG("Minimum size: %d (width) x %d (height)\n", size_hints.min_width, size_hints.min_height);

        ASSIGN_IF_CHANGED(win->min_width, size_hints.min_width);
        ASSIGN_IF_CHANGED(win->min_height, size_hints.min_height);
    }

    if ((size_hints.flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) {
        DLOG("Maximum size: %d (width) x %d (height)\n", size_hints.max_width, size_hints.max_height);

        int max_width = max(0, size_hints.max_width);
        int max_height = max(0, size_hints.max_height);

        ASSIGN_IF_CHANGED(win->max_width, max_width);
        ASSIGN_IF_CHANGED(win->max_height, max_height);
    } else {
        DLOG("Clearing maximum size\n");

        ASSIGN_IF_CHANGED(win->max_width, 0);
        ASSIGN_IF_CHANGED(win->max_height, 0);
    }

    if ((size_hints.flags & XCB_ICCCM_SIZE_HINT_P_RESIZE_INC)) {
        DLOG("Size increments: %d (width) x %d (height)\n", size_hints.width_inc, size_hints.height_inc);

        if (size_hints.width_inc > 0 && size_hints.width_inc < 0xFFFF) {
            ASSIGN_IF_CHANGED(win->width_increment, size_hints.width_inc);
        } else {
            ASSIGN_IF_CHANGED(win->width_increment, 0);
        }

        if (size_hints.height_inc > 0 && size_hints.height_inc < 0xFFFF) {
            ASSIGN_IF_CHANGED(win->height_increment, size_hints.height_inc);
        } else {
            ASSIGN_IF_CHANGED(win->height_increment, 0);
        }
    } else {
        DLOG("Clearing size increments\n");

        ASSIGN_IF_CHANGED(win->width_increment, 0);
        ASSIGN_IF_CHANGED(win->height_increment, 0);
    }

    /* The base width / height is the desired size of the window. */
    if (size_hints.flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE &&
        (win->base_width >= 0) && (win->base_height >= 0)) {
        DLOG("Base size: %d (width) x %d (height)\n", size_hints.base_width, size_hints.base_height);

        ASSIGN_IF_CHANGED(win->base_width, size_hints.base_width);
        ASSIGN_IF_CHANGED(win->base_height, size_hints.base_height);
    } else {
        DLOG("Clearing base size\n");

        ASSIGN_IF_CHANGED(win->base_width, 0);
        ASSIGN_IF_CHANGED(win->base_height, 0);
    }

    if (geom != NULL &&
        (size_hints.flags & XCB_ICCCM_SIZE_HINT_US_POSITION || size_hints.flags & XCB_ICCCM_SIZE_HINT_P_POSITION) &&
        (size_hints.flags & XCB_ICCCM_SIZE_HINT_US_SIZE || size_hints.flags & XCB_ICCCM_SIZE_HINT_P_SIZE)) {
        DLOG("Setting geometry x=%d y=%d w=%d h=%d\n", size_hints.x, size_hints.y, size_hints.width, size_hints.height);
        geom->x = size_hints.x;
        geom->y = size_hints.y;
        geom->width = size_hints.width;
        geom->height = size_hints.height;
    }

    /* If no aspect ratio was set or if it was invalid, we ignore the hints */
    if (size_hints.flags & XCB_ICCCM_SIZE_HINT_P_ASPECT &&
        (size_hints.min_aspect_num >= 0) && (size_hints.min_aspect_den > 0) &&
        (size_hints.max_aspect_num >= 0) && (size_hints.max_aspect_den > 0)) {
        /* Convert numerator/denominator to a double */
        double min_aspect = (double)size_hints.min_aspect_num / size_hints.min_aspect_den;
        double max_aspect = (double)size_hints.max_aspect_num / size_hints.max_aspect_den;
        DLOG("Aspect ratio set: minimum %f, maximum %f\n", min_aspect, max_aspect);
        if (fabs(win->min_aspect_ratio - min_aspect) > DBL_EPSILON) {
            win->min_aspect_ratio = min_aspect;
            changed = true;
        }
        if (fabs(win->max_aspect_ratio - max_aspect) > DBL_EPSILON) {
            win->max_aspect_ratio = max_aspect;
            changed = true;
        }
    } else {
        DLOG("Clearing aspect ratios\n");

        ASSIGN_IF_CHANGED(win->min_aspect_ratio, 0.0);
        ASSIGN_IF_CHANGED(win->max_aspect_ratio, 0.0);
    }

    return changed;
}

/*
 * Updates the WM_HINTS (we only care about the input focus handling part).
 *
 */
void window_update_hints(i3Window *win, xcb_get_property_reply_t *prop, bool *urgency_hint) {
    if (urgency_hint != NULL)
        *urgency_hint = false;

    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("WM_HINTS not set.\n");
        FREE(prop);
        return;
    }

    xcb_icccm_wm_hints_t hints;

    if (!xcb_icccm_get_wm_hints_from_reply(&hints, prop)) {
        DLOG("Could not get WM_HINTS\n");
        free(prop);
        return;
    }

    if (hints.flags & XCB_ICCCM_WM_HINT_INPUT) {
        win->doesnt_accept_focus = !hints.input;
        LOG("WM_HINTS.input changed to \"%d\"\n", hints.input);
    }

    if (urgency_hint != NULL)
        *urgency_hint = (xcb_icccm_wm_hints_get_urgency(&hints) != 0);

    free(prop);
}

/*
 * Updates the MOTIF_WM_HINTS. The container's border style should be set to
 * `motif_border_style' if border style is not BS_NORMAL.
 *
 * i3 only uses this hint when it specifies a window should have no
 * title bar, or no decorations at all, which is how most window managers
 * handle it.
 *
 * The EWMH spec intended to replace Motif hints with _NET_WM_WINDOW_TYPE, but
 * it is still in use by popular widget toolkits such as GTK+ and Java AWT.
 *
 */
bool window_update_motif_hints(i3Window *win, xcb_get_property_reply_t *prop, border_style_t *motif_border_style) {
    /* See `man VendorShell' for more info, `XmNmwmDecorations' section:
     * https://linux.die.net/man/3/vendorshell
     * The following constants are adapted from <Xm/MwmUtil.h>.
     */
#define MWM_HINTS_FLAGS_FIELD 0
#define MWM_HINTS_DECORATIONS_FIELD 2

#define MWM_HINTS_DECORATIONS (1 << 1)
#define MWM_DECOR_ALL (1 << 0)
#define MWM_DECOR_BORDER (1 << 1)
#define MWM_DECOR_TITLE (1 << 3)

    if (motif_border_style != NULL)
        *motif_border_style = BS_NORMAL;

    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        FREE(prop);
        return false;
    }

    /* The property consists of an array of 5 uint32_t's. The first value is a
     * bit mask of what properties the hint will specify. We are only interested
     * in MWM_HINTS_DECORATIONS because it indicates that the third value of the
     * array tells us which decorations the window should have, each flag being
     * a particular decoration. Notice that X11 (Xlib) often mentions 32-bit
     * fields which in reality are implemented using unsigned long variables
     * (64-bits long on amd64 for example). On the other hand,
     * xcb_get_property_value() behaves strictly according to documentation,
     * i.e. returns 32-bit data fields. */
    uint32_t *motif_hints = (uint32_t *)xcb_get_property_value(prop);

    if (motif_border_style != NULL &&
        motif_hints[MWM_HINTS_FLAGS_FIELD] & MWM_HINTS_DECORATIONS) {
        if (motif_hints[MWM_HINTS_DECORATIONS_FIELD] & MWM_DECOR_ALL ||
            motif_hints[MWM_HINTS_DECORATIONS_FIELD] & MWM_DECOR_TITLE)
            *motif_border_style = BS_NORMAL;
        else if (motif_hints[MWM_HINTS_DECORATIONS_FIELD] & MWM_DECOR_BORDER)
            *motif_border_style = BS_PIXEL;
        else
            *motif_border_style = BS_NONE;
    }

    FREE(prop);
    return true;

#undef MWM_HINTS_FLAGS_FIELD
#undef MWM_HINTS_DECORATIONS_FIELD
#undef MWM_HINTS_DECORATIONS
#undef MWM_DECOR_ALL
#undef MWM_DECOR_BORDER
#undef MWM_DECOR_TITLE
}

/*
 * Updates the WM_CLIENT_MACHINE
 *
 */
void window_update_machine(i3Window *win, xcb_get_property_reply_t *prop) {
    if (prop == NULL || xcb_get_property_value_length(prop) == 0) {
        DLOG("WM_CLIENT_MACHINE not set.\n");
        FREE(prop);
        return;
    }

    FREE(win->machine);
    win->machine = sstrndup((char *)xcb_get_property_value(prop), xcb_get_property_value_length(prop));
    LOG("WM_CLIENT_MACHINE changed to \"%s\"\n", win->machine);

    free(prop);
}

void window_update_icon(i3Window *win, xcb_get_property_reply_t *prop) {
    uint32_t *data = NULL;
    uint32_t width, height;
    uint64_t len = 0;
    const uint32_t pref_size = (uint32_t)(render_deco_height() - logical_px(2));

    if (!prop || prop->type != XCB_ATOM_CARDINAL || prop->format != 32) {
        DLOG("_NET_WM_ICON is not set\n");
        FREE(prop);
        return;
    }

    uint32_t prop_value_len = xcb_get_property_value_length(prop);
    uint32_t *prop_value = (uint32_t *)xcb_get_property_value(prop);

    /* Find an icon matching the preferred size.
     * If there is no such icon, take the smallest icon having at least
     * the preferred size.
     * If all icons are smaller than the preferred size, chose the largest.
     */
    while (prop_value_len > (sizeof(uint32_t) * 2) && prop_value &&
           prop_value[0] && prop_value[1]) {
        const uint32_t cur_width = prop_value[0];
        const uint32_t cur_height = prop_value[1];
        /* Check that the property is as long as it should be (in bytes),
           handling integer overflow. "+2" to handle the width and height
           fields. */
        const uint64_t cur_len = cur_width * (uint64_t)cur_height;
        const uint64_t expected_len = (cur_len + 2) * 4;

        if (expected_len > prop_value_len) {
            break;
        }

        DLOG("Found _NET_WM_ICON of size: (%d,%d)\n", cur_width, cur_height);

        const bool at_least_preferred_size = (cur_width >= pref_size &&
                                              cur_height >= pref_size);
        const bool smaller_than_current = (cur_width < width ||
                                           cur_height < height);
        const bool larger_than_current = (cur_width > width ||
                                          cur_height > height);
        const bool not_yet_at_preferred = (width < pref_size ||
                                           height < pref_size);
        if (len == 0 ||
            (at_least_preferred_size &&
             (smaller_than_current || not_yet_at_preferred)) ||
            (!at_least_preferred_size &&
             not_yet_at_preferred &&
             larger_than_current)) {
            len = cur_len;
            width = cur_width;
            height = cur_height;
            data = prop_value;
        }

        if (width == pref_size && height == pref_size) {
            break;
        }

        /* Find pointer to next icon in the reply. */
        prop_value_len -= expected_len;
        prop_value = (uint32_t *)(((uint8_t *)prop_value) + expected_len);
    }

    if (!data) {
        DLOG("Could not get _NET_WM_ICON\n");
        FREE(prop);
        return;
    }

    DLOG("Using icon of size (%d,%d) (preferred size: %d)\n",
         width, height, pref_size);

    win->name_x_changed = true; /* trigger a redraw */

    uint32_t *icon = smalloc(len * 4);

    for (uint64_t i = 0; i < len; i++) {
        uint8_t r, g, b, a;
        const uint32_t pixel = data[2 + i];
        a = (pixel >> 24) & 0xff;
        r = (pixel >> 16) & 0xff;
        g = (pixel >> 8) & 0xff;
        b = (pixel >> 0) & 0xff;

        /* Cairo uses premultiplied alpha */
        r = (r * a) / 0xff;
        g = (g * a) / 0xff;
        b = (b * a) / 0xff;

        icon[i] = ((uint32_t)a << 24) | (r << 16) | (g << 8) | b;
    }

    if (win->icon != NULL) {
        cairo_surface_destroy(win->icon);
    }
    win->icon = cairo_image_surface_create_for_data(
        (unsigned char *)icon,
        CAIRO_FORMAT_ARGB32,
        width,
        height,
        width * 4);
    static cairo_user_data_key_t free_data;
    cairo_surface_set_user_data(win->icon, &free_data, icon, free);

    FREE(prop);
}
