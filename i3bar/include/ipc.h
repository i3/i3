#ifndef IPC_H_
#define IPC_H_

#include <ev.h>
#include <stdint.h>

int init_connection(const char *socket_path);
int i3_send_msg(uint32_t type, const char* payload);
void subscribe_events();

#endif
