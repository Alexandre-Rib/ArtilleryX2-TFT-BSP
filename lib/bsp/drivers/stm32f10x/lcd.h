#ifndef _LCD_H_
#define _LCD_H_

#include <stdint.h>
#include "mks_tft28.h"  // for STM32_HAS_FSMC etc.

#ifdef STM32_HAS_FSMC
  typedef struct
  {
    volatile uint16_t LCD_REG;
    volatile uint16_t LCD_RAM;
  } LCD_TypeDef;

  #define LCD_BASE           ((uint32_t)(0x60000000 | 0x00FFFFFE))
  #define LCD                ((LCD_TypeDef *) LCD_BASE)

  #define LCD_WR_REG(regval) do{ LCD->LCD_REG = regval; }while(0)
  #define LCD_WR_DATA(data)  do{ LCD->LCD_RAM = data; }while(0)

  static inline void LCD_FillColor(uint16_t color, uint32_t count)
  { while (count--) LCD_WR_DATA(color); }

#else
  void LCD_WR_REG(uint16_t data);
  void LCD_WR_DATA(uint16_t data);
  void LCD_FillColor(uint16_t color, uint32_t count);
#endif

uint16_t LCD_RD_DATA(void);
void LCD_HardwareConfig(void);

#endif
