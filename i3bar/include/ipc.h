#ifndef IPC_H_
#define IPC_H_

#include <ev.h>

ev_io*	i3_events;
ev_io*	outputs_watcher;
ev_io*	workspaces_watcher;

void init_i3(const char* socket_path);
void get_outputs_json(void (*callback)(char*, void*), void* params);
void get_workspaces_json(void (*callback)(char*, void*), void* params);

#endif
