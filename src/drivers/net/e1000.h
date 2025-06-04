#ifndef _E1000_H_
#define _E1000_H_
#include <stdint.h>
void e1000_init(void);
int e1000_send(const uint8_t *data, uint16_t len);
int e1000_receive(uint8_t *buf, uint16_t buf_len);
#endif
