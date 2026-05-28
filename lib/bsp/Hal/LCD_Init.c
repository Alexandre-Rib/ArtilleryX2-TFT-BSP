#include "LCD_Init.h"
#include "LCD_Colors.h"
#include "GPIO_Init.h"
#include "delay.h"
#include "LCD_Driver/HX8558.h"

static void (* pLCD_SetDirection)(uint8_t rotate);
static void (* pLCD_SetWindow)(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey);

#ifdef LCD_LED_PIN

static inline void LCD_LED_On(void)
{
  #ifdef LCD_LED_PWM_CHANNEL
    LCD_SET_BRIGHTNESS(100);
  #else
    GPIO_SetLevel(LCD_LED_PIN, 1);
  #endif
}

static inline void LCD_LED_Off(void)
{
  #ifdef LCD_LED_PWM_CHANNEL
    LCD_SET_BRIGHTNESS(0);
  #else
    GPIO_SetLevel(LCD_LED_PIN, 0);
  #endif
}

static inline void LCD_LED_Init(void)
{
  #ifdef LCD_LED_PWM_CHANNEL
    GPIO_InitSet(LCD_LED_PIN, MGPIO_MODE_AF_PP, LCD_LED_PIN_ALTERNATE);
    TIM_PWM_Init(LCD_LED_PWM_CHANNEL);
  #else
    LCD_LED_Off();
    GPIO_InitSet(LCD_LED_PIN, MGPIO_MODE_OUT_PP, 0);
  #endif
}

#endif  // LCD_LED_PIN

static inline void LCD_Init_Sequential(void)
{
  HX8558_Init_Sequential();
  pLCD_SetDirection = HX8558_SetDirection;
  pLCD_SetWindow    = HX8558_SetWindow;
}

void LCD_Init(void)
{
  LCD_HardwareConfig();
  LCD_Init_Sequential();
  LCD_RefreshDirection(0);

  LCD_SetWindow(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
  for (uint32_t i = 0; i < (uint32_t)LCD_WIDTH * LCD_HEIGHT; i++)
    LCD_WR_16BITS_DATA(BLACK);

  Delay_ms(120);

  #ifdef LCD_LED_PIN
    LCD_LED_Init();
    LCD_LED_On();
  #endif

  #ifdef STM32_HAS_FSMC
    LCD_DMA_Config();
  #endif
}

void LCD_RefreshDirection(uint8_t rotate)
{
  pLCD_SetDirection(rotate);
}

void LCD_SetWindow(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey)
{
  pLCD_SetWindow(sx, sy, ex, ey);
}
