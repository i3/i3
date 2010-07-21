#include <stdio.h>
#include <ev.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <i3/ipc.h>

#include "common.h"
#include "ipc.h"

struct callback_t {
	void			(*callback)(char*, void*);
	void*			params;
	struct callback_t*	next;
};

struct callback_t* outputs_cb_queue;
struct callback_t* workspaces_cb_queue;

int get_ipc_fd(const char* socket_path) {
	int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (sockfd == -1) {
		printf("ERROR: Could not create Socket!\n");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_LOCAL;
	strcpy(addr.sun_path, socket_path);
	if (connect(sockfd, (const struct sockaddr*) &addr, sizeof(struct sockaddr_un)) < 0) {
		printf("ERROR: Could not connct to i3\n");
		exit(EXIT_FAILURE);
	}
	return sockfd;
}

void get_outputs_cb(struct ev_loop* loop, ev_io *watcher, int revents) {

}

void init_i3(const char* socket_path) {
	int sockfd = get_ipc_fd(socket_path);

	struct get_outputs_callback* cb = malloc(sizeof(struct get_outputs_callback));
	cb->callback = callback;
	cb->params = params;

	ev_io* get_outputs_write = malloc(sizeof(ev_io));

	ev_io_init(get_outputs_write, &get_outputs_write_cb, sockfd, EV_WRITE);
	get_outputs_write->data = (void*) cb;
	ev_io_start(main_loop, get_outputs_write);

	ev_io* get_outputs_read = malloc(sizeof(ev_io));
	ev_io_init(get_outputs_read, &get_outputs_read_cb, sockfd, EV_READ);
	get_outputs_read->data = (void*) cb;
	ev_io_start(main_loop, get_outputs_read);
}


void get_outputs_write_cb(struct ev_loop* loop, ev_io *watcher, int revents) {
	ev_io_stop(loop, watcher);

	int buffer_size = strlen(I3_IPC_MAGIC) + sizeof(uint32_t) + sizeof(uint32_t);
	char msg[buffer_size];
	char *walk = msg;
	uint32_t msg_size = 0;
	uint32_t msg_type = I3_IPC_MESSAGE_TYPE_GET_OUTPUTS;
	int sockfd = watcher->fd;

	strcpy(walk, I3_IPC_MAGIC);
	walk += strlen(I3_IPC_MAGIC);
	memcpy(walk, &msg_size, sizeof(uint32_t));
	walk += sizeof(uint32_t);
	memcpy(walk, &msg_type, sizeof(uint32_t));
	
	int sent_bytes = 0;
	int bytes_to_go = buffer_size;
	while (sent_bytes < bytes_to_go) {
		int n = write(sockfd, msg + sent_bytes, bytes_to_go);
		if (n == -1) {
			printf("ERROR: write() failed!\n");
			exit(EXIT_FAILURE);
		}

		sent_bytes += n;
		bytes_to_go -= n;
	}
	FREE(watcher);
}

void get_outputs_read_cb(struct ev_loop* loop, ev_io *watcher, int revents) {
	ev_io_stop(loop, watcher);

	int to_read = strlen(I3_IPC_MAGIC) + sizeof(uint32_t) + sizeof(uint32_t);
	char msg[to_read];
	char *walk = msg;
	int sockfd = watcher->fd;
	uint8_t *reply;
	struct get_outputs_callback* cb = watcher->data;

	uint32_t reply_length;

	uint32_t read_bytes = 0;
	while (read_bytes < to_read) {
		int n = read(sockfd, msg + read_bytes, to_read);
		if (n == -1) {
			printf("ERROR: read() failed!\n");
			exit(EXIT_FAILURE);
		}
		if (n == 0) {
			printf("ERROR: No reply!\n");
			exit(EXIT_FAILURE);
		}

		read_bytes += n;
		to_read -= n;
	}

	if (memcmp(walk, I3_IPC_MAGIC, strlen(I3_IPC_MAGIC)) != 0) {
		printf("ERROR: Wrong magic!\n");
		exit(EXIT_FAILURE);
	}

	walk += strlen(I3_IPC_MAGIC);
	reply_length = *((uint32_t*) walk);
	walk += sizeof(uint32_t);
	if (*((uint32_t*) walk) != I3_IPC_MESSAGE_TYPE_GET_OUTPUTS) {
		printf("ERROR: Wrong reply type (%d) expected %d!\n",
			*((uint32_t*) walk),
			I3_IPC_MESSAGE_TYPE_GET_OUTPUTS);
		exit(EXIT_FAILURE);
	}
	walk += sizeof(uint32_t);

	reply = malloc(reply_length);
	if (reply == NULL) {
		printf("ERROR: malloc() failed!\n");
		exit(EXIT_FAILURE);
	}

	to_read = reply_length;
	read_bytes = 0;
	while (read_bytes < to_read) {
		int n = read(sockfd, reply + read_bytes, to_read);
		if (n == -1) {
			printf("ERROR: read() failed!\n");
			exit(EXIT_FAILURE);
		}

		read_bytes += n;
		to_read -= n;
	}
	
	cb->callback((char*) reply, cb->params);
	FREE(cb);
	FREE(watcher);
}

void get_outputs_json(void (*callback)(char*, void*), void* params) {
}



struct get_workspaces_callback {
	void (*callback)(char*, void*);
	void* params;
};

void get_workspaces_write_cb(struct ev_loop* loop, ev_io *watcher, int revents) {
	ev_io_stop(loop, watcher);
	//FREE(watcher);

	int buffer_size = strlen(I3_IPC_MAGIC) + sizeof(uint32_t) + sizeof(uint32_t);
	char msg[buffer_size];
	char *walk = msg;
	uint32_t msg_size = 0;
	uint32_t msg_type = I3_IPC_MESSAGE_TYPE_GET_WORKSPACES;
	int sockfd = watcher->fd;

	strcpy(walk, I3_IPC_MAGIC);
	walk += strlen(I3_IPC_MAGIC);
	memcpy(walk, &msg_size, sizeof(uint32_t));
	walk += sizeof(uint32_t);
	memcpy(walk, &msg_type, sizeof(uint32_t));
	
	int sent_bytes = 0;
	int bytes_to_go = buffer_size;
	while (sent_bytes < bytes_to_go) {
		int n = write(sockfd, msg + sent_bytes, bytes_to_go);
		if (n == -1) {
			printf("ERROR: write() failed!\n");
			exit(EXIT_FAILURE);
		}

		sent_bytes += n;
		bytes_to_go -= n;
	}
	FREE(watcher);
}

void get_workspaces_read_cb(struct ev_loop* loop, ev_io *watcher, int revents) {
	ev_io_stop(loop, watcher);
	//FREE(watcher);

	int to_read = strlen(I3_IPC_MAGIC) + sizeof(uint32_t) + sizeof(uint32_t);
	char msg[to_read];
	char *walk = msg;
	int sockfd = watcher->fd;
	uint8_t *reply;
	struct get_workspaces_callback* cb = watcher->data;

	uint32_t reply_length;

	uint32_t read_bytes = 0;
	while (read_bytes < to_read) {
		int n = read(sockfd, msg + read_bytes, to_read);
		if (n == -1) {
			printf("ERROR: read() failed!\n");
			exit(EXIT_FAILURE);
		}
		if (n == 0) {
			printf("ERROR: No reply!\n");
			exit(EXIT_FAILURE);
		}

		read_bytes += n;
		to_read -= n;
	}

	if (memcmp(walk, I3_IPC_MAGIC, strlen(I3_IPC_MAGIC)) != 0) {
		printf("ERROR: Wrong magic!\n");
		exit(EXIT_FAILURE);
	}

	walk += strlen(I3_IPC_MAGIC);
	reply_length = *((uint32_t*) walk);
	walk += sizeof(uint32_t);
	if (*((uint32_t*) walk) != I3_IPC_MESSAGE_TYPE_GET_WORKSPACES) {
		printf("ERROR: Wrong reply type (%d) expected %d!\n",
			*((uint32_t*) walk),
			I3_IPC_MESSAGE_TYPE_GET_WORKSPACES);
		exit(EXIT_FAILURE);
	}
	walk += sizeof(uint32_t);

	reply = malloc(reply_length);
	if (reply == NULL) {
		printf("ERROR: malloc() failed!\n");
		exit(EXIT_FAILURE);
	}

	to_read = reply_length;
	read_bytes = 0;
	while (read_bytes < to_read) {
		int n = read(sockfd, reply + read_bytes, to_read);
		if (n == -1) {
			printf("ERROR: read() failed!\n");
			exit(EXIT_FAILURE);
		}

		read_bytes += n;
		to_read -= n;
	}
	
	cb->callback((char*) reply, cb->params);
	FREE(cb);

	FREE(watcher);
}

void get_workspaces_json(void (*callback)(char*, void*), void* params) {
	socket_path = "/home/mero/.i3/ipc.sock";

	int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (sockfd == -1) {
		printf("ERROR: Could not create Socket!\n");
		exit(EXIT_FAILURE);
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_LOCAL;
	strcpy(addr.sun_path, socket_path);
	if (connect(sockfd, (const struct sockaddr*) &addr, sizeof(struct sockaddr_un)) < 0) {
		printf("ERROR: Could not connct to i3\n");
		exit(EXIT_FAILURE);
	}

	struct get_workspaces_callback* cb = malloc(sizeof(struct get_workspaces_callback));
	cb->callback = callback;
	cb->params = params;

	ev_io* get_workspaces_write = malloc(sizeof(ev_io));

	ev_io_init(get_workspaces_write, &get_workspaces_write_cb, sockfd, EV_WRITE);
	get_workspaces_write->data = (void*) cb;
	ev_io_start(main_loop, get_workspaces_write);

	ev_io* get_workspaces_read = malloc(sizeof(ev_io));
	ev_io_init(get_workspaces_read, &get_workspaces_read_cb, sockfd, EV_READ);
	get_workspaces_read->data = (void*) cb;
	ev_io_start(main_loop, get_workspaces_read);
}
