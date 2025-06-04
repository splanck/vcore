#ifndef _NET_H_
#define _NET_H_
#include <stdint.h>

struct ip_header {
    uint8_t ihl:4;
    uint8_t version:4;
    uint8_t tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed));

struct udp_header {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t checksum;
} __attribute__((packed));

void arp_init(void);
void arp_insert(uint32_t ip, const uint8_t *mac);
int arp_lookup(uint32_t ip, uint8_t *mac);

int ipv4_send(uint32_t dst_ip, uint8_t proto, const uint8_t *data, uint16_t len);
void ipv4_input(const uint8_t *pkt, uint16_t len);

int udp_send(uint32_t dst_ip, uint16_t src_port, uint16_t dst_port, const uint8_t *data, uint16_t len);
void udp_input(uint32_t src_ip, const uint8_t *data, uint16_t len);
int udp_receive(uint32_t *src_ip, uint16_t *src_port, uint8_t *buf, uint16_t buf_len);

int tcp_send(uint32_t dst_ip, const uint8_t *data, uint16_t len);
void tcp_input(uint32_t src_ip, const uint8_t *data, uint16_t len);
int tcp_receive(uint32_t *src_ip, uint8_t *buf, uint16_t buf_len);

#endif
