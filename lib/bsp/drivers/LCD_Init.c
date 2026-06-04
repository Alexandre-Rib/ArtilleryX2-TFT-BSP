#include "LCD_Init.h"
#include "LCD_Colors.h"
#include "GPIO_Init.h"
#include "delay.h"
#include "HX8558.h"

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

void LCD_ReadPixels(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *buf)
{
  // Set window using HX8558_SetWindow (ends with 0x2C write-mode command)
  pLCD_SetWindow((uint16_t)x, (uint16_t)y,
                 (uint16_t)(x + (int16_t)w - 1),
                 (uint16_t)(y + (int16_t)h - 1));
  // Override write-mode with read command (HX8558 uses 0x22 for GRAM read)
  LCD_WR_REG(0x22);
  Delay_us(1);
  LCD_RD_DATA();          // mandatory dummy read
  uint32_t n = (uint32_t)w * h;
  for (uint32_t i = 0; i < n; i++)
    buf[i] = LCD_RD_DATA();
}
