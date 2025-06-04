#include "e1000.h"
#include "kernel/print.h"
#include <string.h>

/*
 * Simplified software implementation of the Intel E1000 driver.
 * It does not touch real hardware but instead provides a tiny
 * loopback device so that the networking stack can be exercised.
 */

#define QUEUE 16
#define PKT_SIZE 1518

static unsigned char tx_buf[QUEUE][PKT_SIZE];
static unsigned short tx_len[QUEUE];
static int tx_head, tx_tail;

static unsigned char rx_buf[QUEUE][PKT_SIZE];
static unsigned short rx_len[QUEUE];
static int rx_head, rx_tail;

void e1000_init(void)
{
    printk("e1000: virtual driver init\n");
    tx_head = tx_tail = 0;
    rx_head = rx_tail = 0;
}

/* Send packet and also loop it back to the receive queue */
int e1000_send(const uint8_t *data, uint16_t len)
{
    if (len > PKT_SIZE)
        len = PKT_SIZE;
    memcpy(tx_buf[tx_tail], data, len);
    tx_len[tx_tail] = len;
    tx_tail = (tx_tail + 1) % QUEUE;

    /* loop back */
    memcpy(rx_buf[rx_tail], data, len);
    rx_len[rx_tail] = len;
    rx_tail = (rx_tail + 1) % QUEUE;
    return len;
}

int e1000_receive(uint8_t *buf, uint16_t buf_len)
{
    if (rx_head == rx_tail)
        return 0;
    uint16_t len = rx_len[rx_head];
    if (len > buf_len)
        len = buf_len;
    memcpy(buf, rx_buf[rx_head], len);
    rx_head = (rx_head + 1) % QUEUE;
    return len;
}
