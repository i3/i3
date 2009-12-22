/*
 * vim:ts=8:expandtab
 *
 * i3 - an improved dynamic tiling window manager
 *
 * © 2009 Michael Stapelberg and contributors
 *
 * See file LICENSE for license information.
 *
 */

#include "data.h"
#include "log.h"

/*
 * Returns the mode of the given container (or MODE_DEFAULT if a NULL pointer
 * was passed in order to save a few explicit checks in other places). If
 * for_frame was set to true, the special case of having exactly one client
 * in a container is handled so that MODE_DEFAULT is returned. For some parts
 * of the rendering, this is interesting, other parts need the real mode.
 *
 */
int container_mode(Container *con, bool for_frame) {
        int num_clients = 0;
        Client *client;

        if (con == NULL || con->mode == MODE_DEFAULT)
                return MODE_DEFAULT;

        if (!for_frame)
                return con->mode;

        CIRCLEQ_FOREACH(client, &(con->clients), clients)
                num_clients++;

        /* If the container contains only one client, mode is irrelevant */
        if (num_clients == 1) {
                DLOG("mode to default\n");
                return MODE_DEFAULT;
        }

        return con->mode;
}
