#include "ipc.h"
#include "outputs.h"
#include "workspaces.h"
#include "common.h"
#include "xcb.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ev.h>

int main(int argc, char **argv) {
	main_loop = ev_default_loop(0);

	init_xcb();

	refresh_outputs(&create_windows, NULL);
	refresh_workspaces(NULL, NULL);

	ev_loop(main_loop, 0);

	ev_default_destroy();
	clean_xcb();
	free_outputs();
	free_workspaces();

	//sleep(5);
	return 0;	
}
