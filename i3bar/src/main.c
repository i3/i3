#include <stdio.h>
#include <i3/ipc.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <ev.h>

#include "common.h"

int main(int argc, char **argv) {
    main_loop = ev_default_loop(0);

    init_xcb();
    init_connection("/home/mero/.i3/ipc.sock");

    subscribe_events();

    i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_OUTPUTS, NULL);
    i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_WORKSPACES, NULL);

    start_child("i3status");

    ev_loop(main_loop, 0);

    kill_child();

    FREE(statusline);

    clean_xcb();
    ev_default_destroy();

    free_workspaces();
    FREE_SLIST(outputs, i3_output);

    return 0;
}
