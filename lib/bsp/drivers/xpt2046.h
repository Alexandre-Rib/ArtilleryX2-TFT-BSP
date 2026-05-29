#ifndef _XPT2046_H_
#define _XPT2046_H_

#include <stdint.h>  // for uint8_t etc.

void XPT2046_Init(uint32_t tpen, uint32_t cs, uint32_t sck, uint32_t miso, uint32_t mosi);
uint8_t XPT2046_Read_Pen(void);
uint16_t XPT2046_Repeated_Compare_AD(uint8_t CMD);

#endif
