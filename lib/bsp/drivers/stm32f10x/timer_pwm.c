#include "timer_pwm.h"
#include "stm32f10x.h"

typedef struct {
  TIM_TypeDef * tim;
  volatile uint32_t * rcc_src;
  uint8_t rcc_bit;
} TIMER;

static const TIMER pwmTimer[_TIM_CNT] = {
  {TIM1,  &RCC->APB2ENR, 11},  // Timer1  APB2 bit11
  {TIM2,  &RCC->APB1ENR, 0},   // Timer2  APB1 bit0
  {TIM3,  &RCC->APB1ENR, 1},   // Timer3  APB1 bit1
  {TIM4,  &RCC->APB1ENR, 2},   // Timer4  APB1 bit2
  {TIM5,  &RCC->APB1ENR, 3},   // Timer5  APB1 bit3
  {TIM6,  &RCC->APB1ENR, 4},   // Timer6  APB1 bit4
  {TIM7,  &RCC->APB1ENR, 5},   // Timer7  APB1 bit5
  {TIM8,  &RCC->APB2ENR, 13},  // Timer8  APB2 bit13
};

void TIM_PWM_SetDutyCycle(uint16_t tim_ch, uint8_t duty)
{
  uint16_t timerIndex = TIMER_GET_TIM(tim_ch);
  uint16_t channel = TIMER_GET_CH(tim_ch);
  const TIMER * timer = &pwmTimer[timerIndex];

  switch (channel)
  {
    case 0: timer->tim->CCR1 = duty; break;
    case 1: timer->tim->CCR2 = duty; break;
    case 2: timer->tim->CCR3 = duty; break;
    case 3: timer->tim->CCR4 = duty; break;
  }
}

void TIM_PWM_Init(uint16_t tim_ch)
{
  uint16_t timerIndex = TIMER_GET_TIM(tim_ch);
  uint16_t channel = TIMER_GET_CH(tim_ch);
  const TIMER * timer = &pwmTimer[timerIndex];
  /* On STM32F107: SYSCLK=72MHz, APB1 prescaler=/2 → APB1 timer clock=72MHz,
     APB2 prescaler=/1 → APB2 timer clock=72MHz. Both equal SystemCoreClock. */
  uint32_t timerTmpClk = SystemCoreClock;

  *timer->rcc_src |= (1 << timer->rcc_bit);

  timer->tim->ARR = 100 - 1;
  timer->tim->PSC = timerTmpClk / (500 * 100) - 1;  // 500 Hz PWM

  switch (channel)
  {
    case 0: timer->tim->CCMR1 |= (6<<4)  | (1<<3);  break;
    case 1: timer->tim->CCMR1 |= (6<<12) | (1<<11); break;
    case 2: timer->tim->CCMR2 |= (6<<4)  | (1<<3);  break;
    case 3: timer->tim->CCMR2 |= (6<<12) | (1<<11); break;
  }

  timer->tim->CCER |= 1 << (4 * channel);
  timer->tim->CR1 = (1 << 7) | (1 << 0);

  if (timer->tim == TIM1 || timer->tim == TIM8)
    timer->tim->BDTR |= 1 << 15;
}
