/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009-2010 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */

#include "all.h"

bool match_is_empty(Match *match) {
    /* we cannot simply use memcmp() because the structure is part of a
     * TAILQ and I don’t want to start with things like assuming that the
     * last member of a struct really is at the end in memory… */
    return (match->title == NULL &&
            match->application == NULL &&
            match->class == NULL &&
            match->instance == NULL &&
            match->id == XCB_NONE &&
            match->con_id == NULL &&
            match->floating == false);
}

bool match_matches_window(Match *match, i3Window *window) {
    /* TODO: pcre, full matching, … */
    if (match->class != NULL && window->class_class != NULL && strcasecmp(match->class, window->class_class) == 0) {
        LOG("match made by window class (%s)\n", window->class_class);
        return true;
    }

    if (match->instance != NULL && window->class_instance != NULL && strcasecmp(match->instance, window->class_instance) == 0) {
        LOG("match made by window instance (%s)\n", window->class_instance);
        return true;
    }


    if (match->id != XCB_NONE && window->id == match->id) {
        LOG("match made by window id (%d)\n", window->id);
        return true;
    }

    LOG("window %d (%s) could not be matched\n", window->id, window->class_class);

    return false;
}

