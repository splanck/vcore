#include "net.h"
#include "kernel/print.h"
#include <string.h>

/* Extremely small and fake TCP implementation used only for demos */
struct tcp_packet {
    uint32_t src_ip;
    uint16_t len;
    uint8_t data[512];
};

#define TCP_QUEUE 8
static struct tcp_packet queue[TCP_QUEUE];
static int t_head = 0, t_tail = 0;

extern int ipv4_send(uint32_t dst_ip, uint8_t proto, const uint8_t *data, uint16_t len);

int tcp_send(uint32_t dst_ip, const uint8_t *data, uint16_t len)
{
    return ipv4_send(dst_ip, 6, data, len);
}

void tcp_input(uint32_t src_ip, const uint8_t *data, uint16_t len)
{
    int next = (t_tail + 1) % TCP_QUEUE;
    if (next == t_head)
        return;
    queue[t_tail].src_ip = src_ip;
    queue[t_tail].len = len > sizeof(queue[t_tail].data) ? sizeof(queue[t_tail].data) : len;
    memcpy(queue[t_tail].data, data, queue[t_tail].len);
    t_tail = next;
}

int tcp_receive(uint32_t *src_ip, uint8_t *buf, uint16_t buf_len)
{
    if (t_head == t_tail)
        return 0;
    if (src_ip)
        *src_ip = queue[t_head].src_ip;
    uint16_t len = queue[t_head].len;
    if (len > buf_len)
        len = buf_len;
    memcpy(buf, queue[t_head].data, len);
    t_head = (t_head + 1) % TCP_QUEUE;
    return len;
}
