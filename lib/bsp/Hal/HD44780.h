#ifndef _HD44780_H
#define _HD44780_H

#include <stdbool.h>
#include <stdint.h>         // for uint8_t etc.
#include "mks_tft28.h"       // for LCD2004_EMULATOR etc.
#include "CircularQueue.h"  // for CIRCULAR_QUEUE etc.

#ifdef LCD2004_EMULATOR
  void HD44780_DeConfig(void);
  void HD44780_Config(CIRCULAR_QUEUE * queue);
  bool HD44780_writeData(void);
  bool HD44780_getData(uint8_t * data);
#endif

#endif
