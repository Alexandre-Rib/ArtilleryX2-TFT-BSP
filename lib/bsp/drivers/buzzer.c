/**
 * @file    buzzer.c
 * @brief   Passive buzzer driver — TIM5_CH3 PWM on PA2 (MKS TFT28 BSP)
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#include "buzzer.h"
#include "stm32f10x.h"
#include "delay.h"

/*
 * Passive buzzer on PA2 — TIM5_CH3 (default mapping on STM32F107, no remap).
 *
 * Counter clocked at 1 MHz (PSC = 71, APB1 clock x2 = 72 MHz).
 *
 * Frequency : F = 1 000 000 / (ARR + 1)   -> range ~30 Hz - 20 kHz
 * Volume    : duty cycle = CCR3 / (ARR + 1)
 *               volume=100 -> duty=50% -> max excursion -> max volume
 *               volume=0   -> duty=0%  -> silence
 * The volume/duty relationship is not linear on a passive buzzer, but the
 * effect is audible and allows envelope shaping (attack/decay/fade-out).
 */

#define BUZZER_COUNTER_HZ  1000000UL   // 1 MHz after prescaler
#define BUZZER_PSC         71u          // 72 MHz / 72 = 1 MHz

void Buzzer_Init(void)
{
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;

  // PA2: AF push-pull 50 MHz
  GPIOA->CRL &= ~(0xFu << 8);
  GPIOA->CRL |=  (0xBu << 8);  // MODE=11 (50 MHz), CNF=10 (AF push-pull)

  TIM5->PSC   = BUZZER_PSC;
  TIM5->ARR   = 999;
  TIM5->CCR3  = 0;
  TIM5->CCMR2 = (6u << 4) | (1u << 3);  // CH3: PWM mode 1, preload enable
  TIM5->CCER  = 0;                        // output disabled at startup
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

  // duty = volume/100 * 50%  ->  CCR = (ARR+1) * volume / 200
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
