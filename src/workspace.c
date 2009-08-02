/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 * workspace.c: Functions for modifying workspaces
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <err.h>

#include "util.h"
#include "data.h"
#include "i3.h"
#include "config.h"
#include "xcb.h"

/*
 * Sets the name (or just its number) for the given workspace. This has to
 * be called for every workspace as the rendering function
 * (render_internal_bar) relies on workspace->name and workspace->name_len
 * being ready-to-use.
 *
 */
void workspace_set_name(Workspace *ws, const char *name) {
        char *label;
        int ret;

        if (name != NULL)
                ret = asprintf(&label, "%d: %s", ws->num + 1, name);
        else ret = asprintf(&label, "%d", ws->num + 1);

        if (ret == -1)
                errx(1, "asprintf() failed");

        FREE(ws->name);

        ws->name = convert_utf8_to_ucs2(label, &(ws->name_len));
        ws->text_width = predict_text_width(global_conn, config.font, ws->name, ws->name_len);

        free(label);
}
