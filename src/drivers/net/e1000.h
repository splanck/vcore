#ifndef _E1000_H_
#define _E1000_H_
#include <stdint.h>
/* Simple Intel E1000 driver interface used by the network stack.  The
 * implementation performs very small scale initialisation of the device
 * and provides DMA based send/receive helpers.  Only a subset of the real
 * hardware features is implemented.  */

void e1000_init(void);
int  e1000_send(const uint8_t *data, uint16_t len);
int  e1000_receive(uint8_t *buf, uint16_t buf_len);
void e1000_interrupt(void);

/* Register offsets used by the minimal driver */
#define E1000_TDBAL   0x03800
#define E1000_TDBAH   0x03804
#define E1000_TDLEN   0x03808
#define E1000_TDH     0x03810
#define E1000_TDT     0x03818
#define E1000_TCTL    0x00400
#define E1000_TIPG    0x00410

#define E1000_RDBAL   0x02800
#define E1000_RDBAH   0x02804
#define E1000_RDLEN   0x02808
#define E1000_RDH     0x02810
#define E1000_RDT     0x02818
#define E1000_RCTL    0x00100

#define E1000_IMS     0x000D0
#define E1000_ICR     0x000C0
#endif
