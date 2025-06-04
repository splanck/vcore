#ifndef _SOCKET_H_
#define _SOCKET_H_
#include <stdint.h>

int socket_create(int type);
int socket_send(int sock, const void *buf, int len);
int socket_recv(int sock, void *buf, int len);

#endif
