#include "buzzer.h"
#include "stm32f10x.h"
#include "delay.h"

/*
 * Buzzer passif sur PA2 — TIM5_CH3 (mapping par défaut STM32F107, pas de remap).
 *
 * Compteur à 1 MHz (PSC = 71, horloge APB1 × 2 = 72 MHz).
 *
 * Fréquence : F = 1 000 000 / (ARR + 1)   → plage ~30 Hz – 20 kHz
 * Volume    : duty cycle CCR3 / (ARR + 1)
 *               volume=100 → duty=50% → excursion max → volume max
 *               volume=0   → duty=0%  → silence
 * La relation volume/duty n'est pas linéaire sur un buzzer passif, mais
 * l'effet est audible et permet des enveloppes (attack/decay/fade-out).
 */

#define BUZZER_COUNTER_HZ  1000000UL   // 1 MHz après prescaler
#define BUZZER_PSC         71u          // 72 MHz / 72 = 1 MHz

void Buzzer_Init(void)
{
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;

  // PA2 : AF push-pull 50 MHz
  GPIOA->CRL &= ~(0xFu << 8);
  GPIOA->CRL |=  (0xBu << 8);  // MODE=11 (50 MHz), CNF=10 (AF push-pull)

  TIM5->PSC   = BUZZER_PSC;
  TIM5->ARR   = 999;
  TIM5->CCR3  = 0;
  TIM5->CCMR2 = (6u << 4) | (1u << 3);  // CH3 : PWM mode 1, preload
  TIM5->CCER  = 0;                        // sortie désactivée au départ
  TIM5->EGR   = TIM_EGR_UG;
  TIM5->CR1   = TIM_CR1_ARPE | TIM_CR1_CEN;
}

void Buzzer_Stop(void)
{
  TIM5->CCER &= ~TIM_CCER_CC3E;
}

void Buzzer_Set(uint16_t hz, uint8_t volume)
{
  if (hz == 0 || volume == 0)
  {
    Buzzer_Stop();
    return;
  }

  uint32_t arr = (BUZZER_COUNTER_HZ / hz) - 1u;
  if (arr > 0xFFFFu) arr = 0xFFFFu;

  // duty = volume/100 * 50%  →  CCR = (ARR+1) * volume / 200
  uint32_t ccr = ((arr + 1u) * volume) / 200u;

  TIM5->ARR  = (uint16_t) arr;
  TIM5->CCR3 = (uint16_t) ccr;
  TIM5->EGR  = TIM_EGR_UG;
  TIM5->CCER |= TIM_CCER_CC3E;
}

void Buzzer_PlayTone(uint16_t hz, uint8_t volume, uint16_t ms)
{
  Buzzer_Set(hz, volume);
  Delay_ms(ms);
  Buzzer_Stop();
}

void Buzzer_Beep(void)
{
  Buzzer_PlayTone(2000, 100, 50);
}
