#include "net.h"
#include "drivers/net/e1000.h"
#include "kernel/print.h"
#include <string.h>

static uint8_t host_mac[6] = {0x02,0x00,0x00,0x00,0x00,0x02};
static uint32_t host_ip = 0xc0a80002; /* 192.168.0.2 */

struct eth_header {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t type;
} __attribute__((packed));

static uint16_t ip_id = 1;

extern void udp_input(uint32_t src_ip, const uint8_t *data, uint16_t len);
extern void tcp_input(uint32_t src_ip, const uint8_t *data, uint16_t len);
extern int arp_lookup(uint32_t ip, uint8_t *mac);

int ipv4_send(uint32_t dst_ip, uint8_t proto, const uint8_t *data, uint16_t len)
{
    uint8_t mac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
    arp_lookup(dst_ip, mac);

    uint8_t frame[1500];
    struct eth_header *eth = (struct eth_header*)frame;
    memcpy(eth->dst, mac, 6);
    memcpy(eth->src, host_mac, 6);
    eth->type = 0x0008; /* IP little endian */

    struct ip_header *ip = (struct ip_header*)(frame + sizeof(*eth));
    ip->version = 4;
    ip->ihl = 5;
    ip->tos = 0;
    ip->total_length = sizeof(struct ip_header) + len;
    ip->id = ip_id++;
    ip->flags_fragment = 0;
    ip->ttl = 64;
    ip->protocol = proto;
    ip->checksum = 0;
    ip->src_ip = host_ip;
    ip->dst_ip = dst_ip;

    uint16_t total = sizeof(struct eth_header) + sizeof(struct ip_header) + len;
    memcpy(frame + sizeof(struct eth_header) + sizeof(struct ip_header), data, len);
    return e1000_send(frame, total);
}

void ipv4_input(const uint8_t *pkt, uint16_t len)
{
    if (len < sizeof(struct ip_header))
        return;
    const struct ip_header *ip = (const struct ip_header*)pkt;
    uint16_t hl = ip->ihl * 4;
    if (len < hl)
        return;
    const uint8_t *payload = pkt + hl;
    uint16_t plen = len - hl;
    switch (ip->protocol) {
    case 17:
        udp_input(ip->src_ip, payload, plen);
        break;
    case 6:
        tcp_input(ip->src_ip, payload, plen);
        break;
    }
}
