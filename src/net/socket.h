#ifndef _SOCKET_H_
#define _SOCKET_H_
#include <stdint.h>

#define SOCK_RAW    0
#define SOCK_DGRAM  1
#define SOCK_STREAM 2

int socket_create(int type);
int socket_send(int sock, const void *buf, int len);
int socket_recv(int sock, void *buf, int len);

#endif
