#include "e1000.h"
#include "kernel/print.h"
#include "kernel/keyboard.h" /* for in_byte */
#include "kernel/memory.h"
#include "kernel/trap.h" /* for read_cr3 */
#include <string.h>

/* Low level port I/O helpers.  Only what we need for the driver */
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

static inline void pci_write32(uint32_t addr, uint32_t data)
{
    out_byte(0xCF8, addr & 0xFF);
    out_byte(0xCF8 + 1, (addr >> 8) & 0xFF);
    out_byte(0xCF8 + 2, (addr >> 16) & 0xFF);
    out_byte(0xCF8 + 3, (addr >> 24) & 0xFF);
    out_byte(0xCFC, data & 0xFF);
    out_byte(0xCFC + 1, (data >> 8) & 0xFF);
    out_byte(0xCFC + 2, (data >> 16) & 0xFF);
    out_byte(0xCFC + 3, (data >> 24) & 0xFF);
}

/*
 * Very small software model of the Intel E1000 driver.  The previous
 * version only emulated an in-memory loopback device.  The code below
 * extends this into a minimal real driver: it performs a PCI probe,
 * maps the device BAR, sets up descriptor rings and uses the hardware
 * DMA engine for transmit and receive whenever a device is present.
 * When no hardware is detected the old loopback logic is used so that
 * the rest of the stack keeps working in QEMU or on systems without an
 * E1000 NIC.
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
            uint32_t bar0 = pci_read32(addr | 0x10);
            uint64_t mmio = bar0 & ~0xf;
            uint64_t kmap = P2V(read_cr3());
            map_pages(kmap, P2V(mmio), P2V(mmio + PAGE_SIZE), mmio, PTE_P|PTE_W);
            e1000_regs = (volatile uint32_t*)P2V(mmio);
            break;
        }
    }

    if (e1000_regs) {
        printk("e1000: found at %p\n", (void*)e1000_regs);
        memset(tx_desc, 0, sizeof(tx_desc));
        memset(rx_desc, 0, sizeof(rx_desc));

        for (int i = 0; i < QUEUE; i++) {
            tx_desc[i].status = 0;
            rx_desc[i].status = 0;
            rx_desc[i].addr = (uint64_t)rx_buf[i];
        }

        e1000_regs[E1000_TDBAL/4] = V2P(tx_desc);
        e1000_regs[E1000_TDBAH/4] = 0;
        e1000_regs[E1000_TDLEN/4] = sizeof(tx_desc);
        e1000_regs[E1000_TDH/4] = 0;
        e1000_regs[E1000_TDT/4] = 0;

        e1000_regs[E1000_RDBAL/4] = V2P(rx_desc);
        e1000_regs[E1000_RDBAH/4] = 0;
        e1000_regs[E1000_RDLEN/4] = sizeof(rx_desc);
        e1000_regs[E1000_RDH/4] = 0;
        e1000_regs[E1000_RDT/4] = QUEUE - 1;

        e1000_regs[E1000_TCTL/4] = (1<<1) | (1<<3) | (0x40<<12);
        e1000_regs[E1000_TIPG/4] = 10 | (8<<10) | (12<<20);
        e1000_regs[E1000_RCTL/4] = (1<<1) | (1<<15);
        e1000_regs[E1000_IMS/4] = 0x1F6DC;
    } else {
        printk("e1000: no device found, using loopback only\n");
    }

    tx_head = tx_tail = 0;
    rx_head = rx_tail = 0;
}

/* Send a packet using the descriptor ring.  When no hardware is
 * available packets are looped back into the receive queue. */
int e1000_send(const uint8_t *data, uint16_t len)
{
    if (len > PKT_SIZE)
        len = PKT_SIZE;

    memcpy(tx_buf[tx_tail], data, len);
    tx_len[tx_tail] = len;

    if (e1000_regs) {
        tx_desc[tx_tail].addr = V2P(tx_buf[tx_tail]);
        tx_desc[tx_tail].length = len;
        tx_desc[tx_tail].cmd = 0x9; /* RS | EOP */
        tx_desc[tx_tail].status = 0;
        e1000_regs[E1000_TDT/4] = tx_tail;
        while (!(tx_desc[tx_tail].status & 0x1))
            ;
    } else {
        /* Loopback for builds without real hardware */
        memcpy(rx_buf[rx_tail], data, len);
        rx_len[rx_tail] = len;
        rx_tail = (rx_tail + 1) % QUEUE;
    }

    tx_tail = (tx_tail + 1) % QUEUE;
    return len;
}

int e1000_receive(uint8_t *buf, uint16_t buf_len)
{
    if (e1000_regs) {
        if (!(rx_desc[rx_head].status & 0x1))
            return 0;

        uint16_t len = rx_desc[rx_head].length;
        if (len > buf_len)
            len = buf_len;
        memcpy(buf, (void*)(uintptr_t)P2V(rx_desc[rx_head].addr), len);
        rx_desc[rx_head].status = 0;
        e1000_regs[E1000_RDT/4] = rx_head;
        rx_head = (rx_head + 1) % QUEUE;
        return len;
    }

    if (rx_head == rx_tail)
        return 0;

    uint16_t len = rx_len[rx_head];
    if (len > buf_len)
        len = buf_len;
    memcpy(buf, rx_buf[rx_head], len);
    rx_head = (rx_head + 1) % QUEUE;
    return len;
}

/* Interrupt handler used when the NIC raises an interrupt.  The kernel
 * does not hook the device IRQ directly so the driver exposes this
 * function which the generic timer or polling code may call.  It simply
 * acknowledges pending events. */
void e1000_interrupt(void)
{
    if (!e1000_regs)
        return;
    (void)e1000_regs[E1000_ICR/4];
}
