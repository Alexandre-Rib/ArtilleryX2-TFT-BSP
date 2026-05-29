#include "os_timer.h"
#include "stm32f10x.h"

OS_COUNTER os_counter = {0, 1000};

void OS_InitTimerMs(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;

  NVIC_InitStructure.NVIC_IRQChannel = TIM7_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  RCC->APB1ENR |= 1<<5;
  TIM7->ARR = 1000 - 1;
  /* TIM7 is on APB1; when APB1 prescaler > 1, timer clock = SYSCLK */
  TIM7->PSC = SystemCoreClock / 1000000 - 1;
  TIM7->SR  = (uint16_t) ~(1<<0);
  TIM7->DIER |= 1<<0;
  TIM7->CR1 |= 0x01;
}

void TIM7_IRQHandler(void)
{
  if ((TIM7->SR & TIM_SR_UIF) != 0)
  {
    os_counter.ms++;
    os_counter.sec--;

    if (os_counter.sec == 0)
      os_counter.sec = 1000;

    TIM7->SR &= ~TIM_SR_UIF;
  }
}
