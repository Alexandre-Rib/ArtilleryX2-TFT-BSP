#include "HW_Init.h"
#include "stm32f10x.h"
#include "variants.h"

extern void Delay_init(void);
extern void OS_InitTimerMs(void);
extern void W25Qxx_Init(void);
extern void LCD_Init(void);

void HW_Init(void)
{
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

  Delay_init();

  #if defined(MKS_TFT)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_USART2, ENABLE);
  #endif

  OS_InitTimerMs();
  W25Qxx_Init();
  LCD_Init();
}