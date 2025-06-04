#include "net.h"
#include "kernel/print.h"
#include <string.h>

struct udp_packet {
    uint32_t src_ip;
    uint16_t src_port;
    uint16_t len;
    uint8_t data[512];
};

#define UDP_QUEUE 8
static struct udp_packet queue[UDP_QUEUE];
static int q_head = 0, q_tail = 0;

extern int ipv4_send(uint32_t dst_ip, uint8_t proto, const uint8_t *data, uint16_t len);

int udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const uint8_t *data, uint16_t len)
{
    uint8_t pkt[512];
    struct udp_header *uh = (struct udp_header*)pkt;
    uh->src_port = src_port;
    uh->dst_port = dst_port;
    uh->len = sizeof(struct udp_header) + len;
    uh->checksum = 0;
    memcpy(pkt + sizeof(struct udp_header), data, len);
    return ipv4_send(dst_ip, 17, pkt, uh->len);
}

void udp_input(uint32_t src_ip, const uint8_t *data, uint16_t len)
{
    if (len < sizeof(struct udp_header))
        return;
    int next = (q_tail + 1) % UDP_QUEUE;
    if (next == q_head)
        return; /* drop */
    queue[q_tail].src_ip = src_ip;
    const struct udp_header *uh = (const struct udp_header*)data;
    queue[q_tail].src_port = uh->src_port;
    queue[q_tail].len = len - sizeof(struct udp_header);
    if (queue[q_tail].len > sizeof(queue[q_tail].data))
        queue[q_tail].len = sizeof(queue[q_tail].data);
    memcpy(queue[q_tail].data, data + sizeof(struct udp_header), queue[q_tail].len);
    q_tail = next;
}

int udp_receive(uint32_t *src_ip, uint16_t *src_port, uint8_t *buf, uint16_t buf_len)
{
    if (q_head == q_tail)
        return 0;
    if (src_ip)
        *src_ip = queue[q_head].src_ip;
    if (src_port)
        *src_port = queue[q_head].src_port;
    uint16_t len = queue[q_head].len;
    if (len > buf_len)
        len = buf_len;
    memcpy(buf, queue[q_head].data, len);
    q_head = (q_head + 1) % UDP_QUEUE;
    return len;
}
