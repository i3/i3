#ifndef _IPC_H
#define _IPC_H

void ipc_send_message(int sockfd, uint32_t message_size,
                      uint32_t message_type, uint8_t *payload);

int connect_ipc(char *socket_path);

#endif
