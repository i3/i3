/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "libi3.h"

#include <string.h>

/*
 * Returns true if this version of i3 is a debug build (anything which is not a
 * release version), based on the git version number.
 *
 */
bool is_debug_build(void) {
    /* i3_version contains either something like this:
     *     "4.0.2 (2011-11-11)" (release version)
     * or: "4.0.2-123-gC0FFEE"  (debug version)
     *
     * So we check for the offset of the first opening round bracket to
     * determine whether this is a git version or a release version. */
    if (strchr(I3_VERSION, '(') == NULL) {
        return true;  // e.g. 4.0.2-123-gC0FFEE
    }
    /* In practice, debug versions do not contain parentheses at all,
     * but leave the logic as it was before so that we can re-add
     * parentheses if we chose to. */
    return ((strchr(I3_VERSION, '(') - I3_VERSION) > 10);
}
