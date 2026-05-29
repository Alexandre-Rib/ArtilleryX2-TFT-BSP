#include "HW_Init.h"
#include "stm32f10x.h"
#include "mks_tft28.h"
#include "delay.h"
#include "os_timer.h"
#include "w25qxx.h"
#include "LCD_Init.h"
#include "xpt2046.h"

void HW_Init(void)
{
  SystemClockInit();
  SCB->VTOR = VECT_TAB_FLASH;

  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
  GPIO_PinRemapConfig(GPIO_Remap_USART2, ENABLE);

  Delay_init();
  OS_InitTimerMs();
  W25Qxx_Init();
  LCD_Init();
  XPT2046_Init(XPT2046_TPEN, XPT2046_CS, XPT2046_SCK, XPT2046_MISO, XPT2046_MOSI);
}
