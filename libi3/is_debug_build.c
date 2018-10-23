/*
 * vim:ts=4:sw=4:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 * © 2009 Michael Stapelberg and contributors (see also: LICENSE)
 *
 */
#include "libi3.h"

#include <string.h>
#include <stdbool.h>

/*
 * Returns true if this version of i3 is a debug build (anything which is not a
 * release version), based on the git version number.
 *
 */
bool is_debug_build(void) {
    /* i3_version contains either something like this:
     *     "4.0.2 (2011-11-11, branch "release")".
     * or: "4.0.2-123-gCOFFEEBABE (2011-11-11, branch "next")".
     *
     * So we check for the offset of the first opening round bracket to
     * determine whether this is a git version or a release version. */
    return ((strchr(I3_VERSION, '(') - I3_VERSION) > 10);
}
