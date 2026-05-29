#ifndef _BUZZER_H_
#define _BUZZER_H_

#include <stdint.h>    // for uint16_t etc.
#include "mks_tft28.h"  // for BUZZER_PIN etc.

#ifdef BUZZER_PIN
  void Buzzer_Config(void);
  void Buzzer_DeConfig(void);
  void Buzzer_AddSound(const uint16_t frequency, const uint16_t duration);
#endif

#endif
