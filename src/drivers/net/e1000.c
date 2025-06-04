#include "e1000.h"
#include "kernel/print.h"

/*
 * This is a very small stub of the Intel E1000 driver. It only
 * provides minimal hooks used by the network stack. A real driver
 * would need to perform PCI discovery, MMIO setup and interrupt
 * handling. Here we simply provide empty implementations so the rest
 * of the kernel can be compiled and linked.
 */

void e1000_init(void)
{
    printk("e1000: init stub\n");
}

int e1000_send(const uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
    printk("e1000: send stub\n");
    return len;
}

int e1000_receive(uint8_t *buf, uint16_t buf_len)
{
    (void)buf;
    (void)buf_len;
    /* no data available */
    return 0;
}
