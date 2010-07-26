#include <stdio.h>
#include <i3/ipc.h>
#include <string.h>
#include <unistd.h>
#include <ev.h>

#include "ipc.h"
#include "outputs.h"
#include "workspaces.h"
#include "common.h"
#include "xcb.h"

int main(int argc, char **argv) {
	main_loop = ev_default_loop(0);

	init_xcb();
	init_connection("/home/mero/.i3/ipc.sock");

	subscribe_events();

	i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_OUTPUTS, NULL);
	i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_WORKSPACES, NULL);

	ev_loop(main_loop, 0);

	ev_default_destroy();
	clean_xcb();
	free_outputs();
	free_workspaces();

	return 0;	
}
