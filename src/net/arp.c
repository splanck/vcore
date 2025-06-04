#include "net.h"
#include "drivers/net/e1000.h"
#include "kernel/print.h"
#include <string.h>

struct arp_entry {
    uint32_t ip;
    uint8_t mac[6];
};

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
