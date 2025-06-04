#include "e1000.h"
#include "kernel/print.h"
#include "kernel/keyboard.h" /* for in_byte */
#include <string.h>

/* Low level port I/O helpers.  Only what we need for the fake driver */
static inline void out_byte(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" :: "a"(val), "d"(port));
}

static inline uint32_t pci_read32(uint32_t addr)
{
    out_byte(0xCF8, addr & 0xFF);
    out_byte(0xCF8 + 1, (addr >> 8) & 0xFF);
    out_byte(0xCF8 + 2, (addr >> 16) & 0xFF);
    out_byte(0xCF8 + 3, (addr >> 24) & 0xFF);
    uint32_t data =
        in_byte(0xCFC) |
        ((uint32_t)in_byte(0xCFC + 1) << 8) |
        ((uint32_t)in_byte(0xCFC + 2) << 16) |
        ((uint32_t)in_byte(0xCFC + 3) << 24);
    return data;
}

/*
 * Very small software model of the Intel E1000 driver.  The
 * original code only implemented an in-memory loopback device.
 * For demonstration purposes we add a skeleton that looks a bit
 * more like a real driver: we pretend to perform PCI discovery,
 * MMIO mapping and descriptor ring setup.  Real hardware access
 * is not possible in the test environment so the driver still
 * falls back to the old loopback behaviour when actual device
 * registers are not present.
 */

#define QUEUE 16
#define PKT_SIZE 1518

/* Descriptor ring structures used when real hardware is present. */
struct e1000_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
};

struct e1000_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t csum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
};

static struct e1000_tx_desc tx_desc[QUEUE];
static struct e1000_rx_desc rx_desc[QUEUE];

static unsigned char tx_buf[QUEUE][PKT_SIZE];
static unsigned short tx_len[QUEUE];
static int tx_head, tx_tail;

static unsigned char rx_buf[QUEUE][PKT_SIZE];
static unsigned short rx_len[QUEUE];
static int rx_head, rx_tail;

/* Base pointer to the device registers if mapped. */
static volatile uint32_t *e1000_regs;

void e1000_init(void)
{
    printk("e1000: probing for device\n");

    e1000_regs = NULL;
    for (int dev = 0; dev < 32; dev++) {
        uint32_t addr = (1u << 31) | (0 << 16) | (dev << 11) | (0 << 8);
        uint32_t id = pci_read32(addr);
        if (id == 0x100E8086) {
            uint32_t bar = pci_read32(addr | 0x10);
            e1000_regs = (volatile uint32_t*)(bar & ~0xf);
            break;
        }
    }

    if (e1000_regs) {
        printk("e1000: found at %p\n", (void*)e1000_regs);
        memset(tx_desc, 0, sizeof(tx_desc));
        memset(rx_desc, 0, sizeof(rx_desc));
        for (int i = 0; i < QUEUE; i++)
            rx_desc[i].addr = (uint64_t)rx_buf[i];
    } else {
        printk("e1000: no device found, using loopback only\n");
    }

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

    if (e1000_regs) {
        tx_desc[tx_tail].addr = (uint64_t)tx_buf[tx_tail];
        tx_desc[tx_tail].length = len;
        tx_desc[tx_tail].cmd = 0x9; /* RS | EOP */
        /* In a real driver we would update TDT and wait for hardware */
    }

    tx_tail = (tx_tail + 1) % QUEUE;

    /* Without actual hardware we simply loop the packet back */
    memcpy(rx_buf[rx_tail], data, len);
    rx_len[rx_tail] = len;
    rx_tail = (rx_tail + 1) % QUEUE;
    return len;
}

int e1000_receive(uint8_t *buf, uint16_t buf_len)
{
    if (rx_head == rx_tail)
        return 0;

    if (e1000_regs && (rx_desc[rx_head].status & 0x1)) {
        rx_len[rx_head] = rx_desc[rx_head].length;
        memcpy(rx_buf[rx_head], (void*)(uintptr_t)rx_desc[rx_head].addr,
               rx_len[rx_head] > PKT_SIZE ? PKT_SIZE : rx_len[rx_head]);
        rx_desc[rx_head].status = 0;
        /* A real driver would return the buffer to hardware here. */
    }

    uint16_t len = rx_len[rx_head];
    if (len > buf_len)
        len = buf_len;
    memcpy(buf, rx_buf[rx_head], len);
    rx_head = (rx_head + 1) % QUEUE;
    return len;
}

/* Interrupt handler stub.  Real hardware would trigger this when new
 * packets are available or when transmission has completed.  In this
 * toy driver we simply poll the receive descriptors. */
void e1000_interrupt(void)
{
    (void)e1000_regs;
    /* Nothing to do – polling in e1000_receive() handles data */
}
