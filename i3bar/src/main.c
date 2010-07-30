#include <stdio.h>
#include <i3/ipc.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ev.h>
#include <xcb/xcb.h>

#include "ipc.h"
#include "outputs.h"
#include "workspaces.h"
#include "common.h"
#include "xcb.h"

void ev_prepare_cb(struct ev_loop *loop, ev_prepare *w, int revents) {
	xcb_flush(xcb_connection);
}

void ev_check_cb(struct ev_loop *loop, ev_check *w, int revents) {
	xcb_generic_event_t *event;
	if ((event = xcb_poll_for_event(xcb_connection)) != NULL) {
		handle_xcb_event(event);
	}
	free(event);
}

void xcb_io_cb(struct ev_loop *loop, ev_io *w, int revents) {
}

int main(int argc, char **argv) {
	main_loop = ev_default_loop(0);

	init_xcb();
	init_connection("/home/mero/.i3/ipc.sock");

	subscribe_events();

	ev_io *xcb_io = malloc(sizeof(ev_io));
	ev_prepare *ev_prep = malloc(sizeof(ev_prepare));
	ev_check *ev_chk = malloc(sizeof(ev_check));

	ev_io_init(xcb_io, &xcb_io_cb, xcb_get_file_descriptor(xcb_connection), EV_READ);
	ev_prepare_init(ev_prep, &ev_prepare_cb);
	ev_check_init(ev_chk, &ev_check_cb);

	ev_io_start(main_loop, xcb_io);
	ev_prepare_start(main_loop, ev_prep);
	ev_check_start(main_loop, ev_chk);

	i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_OUTPUTS, NULL);
	i3_send_msg(I3_IPC_MESSAGE_TYPE_GET_WORKSPACES, NULL);

	ev_loop(main_loop, 0);

	ev_prepare_stop(main_loop, ev_prep);
	ev_check_stop(main_loop, ev_chk);
	FREE(ev_prep);
	FREE(ev_chk);

	ev_default_destroy();
	clean_xcb();

	free_workspaces();
	FREE_SLIST(outputs, i3_output);

	return 0;	
}
