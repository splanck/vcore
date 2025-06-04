#include "socket.h"
#include "drivers/net/e1000.h"
#include "kernel/print.h"
#include "net.h"
#include <string.h>

/* glue to higher level protocols */
extern int udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const uint8_t *data, uint16_t len);
extern int udp_receive(uint32_t *src_ip, uint16_t *src_port, uint8_t *buf, uint16_t len);
extern int tcp_send(uint32_t dst_ip, const uint8_t *data, uint16_t len);
extern int tcp_receive(uint32_t *src_ip, uint8_t *buf, uint16_t len);
extern void ipv4_input(const uint8_t *pkt, uint16_t len);
extern void arp_input(const uint8_t *pkt, uint16_t len);

#define MAX_SOCKETS 16

struct sock {
    int used;
    int type;
};

static struct sock sockets[MAX_SOCKETS];

static void net_poll(void)
{
    uint8_t pkt[1600];
    int n;
    struct eth_header { uint8_t dst[6]; uint8_t src[6]; uint16_t type; } __attribute__((packed));

    /* acknowledge any pending interrupts and fetch newly received frames */
    e1000_interrupt();

    while ((n = e1000_receive(pkt, sizeof(pkt))) > 0) {
        if (n < sizeof(struct eth_header))
            continue;
        struct eth_header *eth = (struct eth_header*)pkt;
        if (eth->type == 0x0008) {
            ipv4_input(pkt + sizeof(struct eth_header), n - sizeof(struct eth_header));
        } else if (eth->type == 0x0608) {
            arp_input(pkt + sizeof(struct eth_header), n - sizeof(struct eth_header));
        }
    }
}

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
    switch (sockets[sock].type) {
    case SOCK_DGRAM:
        /* send to the host machine at 192.168.0.1 */
        return udp_send(0xc0a80001, 1234, 1234, buf, len);
    case SOCK_STREAM:
        return tcp_send(0xc0a80001, buf, len);
    default:
        return e1000_send(buf, (uint16_t)len);
    }
}

int socket_recv(int sock, void *buf, int len)
{
    if (sock < 0 || sock >= MAX_SOCKETS || !sockets[sock].used)
        return -1;
    net_poll();
    switch (sockets[sock].type) {
    case SOCK_DGRAM:
        return udp_receive(NULL, NULL, buf, len);
    case SOCK_STREAM:
        return tcp_receive(NULL, buf, len);
    default:
        return e1000_receive(buf, (uint16_t)len);
    }
}
