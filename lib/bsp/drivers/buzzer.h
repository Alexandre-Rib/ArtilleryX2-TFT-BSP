#ifndef _BUZZER_H_
#define _BUZZER_H_

#include <stdint.h>

// Initialise TIM5_CH3 sur PA2 en mode PWM
void Buzzer_Init(void);

// Coupe le son immédiatement
void Buzzer_Stop(void);

// Change fréquence + volume sans bloquer.
//   hz     : fréquence en Hz (0 = silence)
//   volume : 0 (silence) → 100 (maximum)
void Buzzer_Set(uint16_t hz, uint8_t volume);

// Joue une fréquence pendant ms millisecondes (bloquant)
void Buzzer_PlayTone(uint16_t hz, uint8_t volume, uint16_t ms);

// Bip court standard (2 kHz, volume max, 50 ms)
void Buzzer_Beep(void);

#endif
