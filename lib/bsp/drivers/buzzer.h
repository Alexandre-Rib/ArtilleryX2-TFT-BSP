/**
 * @file    buzzer.h
 * @brief   Passive buzzer driver — TIM5_CH3 PWM on PA2 (MKS TFT28 BSP)
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#ifndef _BUZZER_H_
#define _BUZZER_H_

#include <stdint.h>

// Initialise TIM5_CH3 on PA2 in PWM mode
void Buzzer_Init(void);

// Stop sound output immediately
void Buzzer_Stop(void);

// Set frequency and volume without blocking.
//   hz     : frequency in Hz (0 = silence)
//   volume : 0 (silent) -> 100 (maximum)
void Buzzer_Set(uint16_t hz, uint8_t volume);

// Play a tone for the given duration in milliseconds (blocking)
void Buzzer_PlayTone(uint16_t hz, uint8_t volume, uint16_t ms);

// Standard short beep (2 kHz, full volume, 50 ms)
void Buzzer_Beep(void);

#endif
