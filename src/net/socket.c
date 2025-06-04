#include "socket.h"
#include "drivers/net/e1000.h"
#include "kernel/print.h"

#define MAX_SOCKETS 16

struct sock {
    int used;
    int type;
};

static struct sock sockets[MAX_SOCKETS];

int socket_create(int type)
{
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!sockets[i].used) {
            sockets[i].used = 1;
            sockets[i].type = type;
            return i;
        }
    }
    return -1;
}

int socket_send(int sock, const void *buf, int len)
{
    if (sock < 0 || sock >= MAX_SOCKETS || !sockets[sock].used)
        return -1;
    return e1000_send(buf, (uint16_t)len);
}

int socket_recv(int sock, void *buf, int len)
{
    if (sock < 0 || sock >= MAX_SOCKETS || !sockets[sock].used)
        return -1;
    return e1000_receive(buf, (uint16_t)len);
}
