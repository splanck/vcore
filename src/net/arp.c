#include "net.h"
#include "drivers/net/e1000.h"
#include "kernel/print.h"
#include <string.h>

struct arp_entry {
    uint32_t ip;
    uint8_t mac[6];
};

struct arp_pkt {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t opcode;
    uint8_t smac[6];
    uint32_t sip;
    uint8_t tmac[6];
    uint32_t tip;
} __attribute__((packed));

static uint8_t local_mac[6] = {0x02,0x00,0x00,0x00,0x00,0x02};
static uint32_t local_ip = 0xc0a80002; /* 192.168.0.2 */

#define ARP_TABLE_SIZE 8
static struct arp_entry table[ARP_TABLE_SIZE];

void arp_init(void)
{
    memset(table, 0, sizeof(table));
}

void arp_insert(uint32_t ip, const uint8_t *mac)
{
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (table[i].ip == 0 || table[i].ip == ip) {
            table[i].ip = ip;
            memcpy(table[i].mac, mac, 6);
            break;
        }
    }
}

int arp_lookup(uint32_t ip, uint8_t *mac)
{
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (table[i].ip == ip) {
            memcpy(mac, table[i].mac, 6);
            return 0;
        }
    }
    return -1;
}

void arp_input(const uint8_t *pkt, uint16_t len)
{
    if (len < sizeof(struct arp_pkt))
        return;
    const struct arp_pkt *ap = (const struct arp_pkt*)pkt;
    if (ap->htype != 1 || ap->ptype != 0x0008)
        return;

    arp_insert(ap->sip, ap->smac);

    if (ap->opcode == 1 && ap->tip == local_ip) {
        /* Reply to ARP request */
        uint8_t frame[60];
        struct arp_pkt *rep = (struct arp_pkt*)frame;
        rep->htype = 1;
        rep->ptype = 0x0008;
        rep->hlen = 6;
        rep->plen = 4;
        rep->opcode = 2;
        memcpy(rep->smac, local_mac, 6);
        rep->sip = local_ip;
        memcpy(rep->tmac, ap->smac, 6);
        rep->tip = ap->sip;

        struct eth_header {
            uint8_t dst[6]; uint8_t src[6]; uint16_t type;
        } __attribute__((packed));
        struct eth_header *eth = (struct eth_header*)frame;
        memcpy(eth->dst, ap->smac, 6);
        memcpy(eth->src, local_mac, 6);
        eth->type = 0x0608;

        e1000_send(frame, sizeof(struct eth_header) + sizeof(struct arp_pkt));
    }
}
