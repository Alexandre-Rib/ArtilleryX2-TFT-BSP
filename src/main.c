/**
 * @file    main.c
 * @brief   Application entry point — BSP bring-up proof of concept
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#include "mks_tft28.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "buzzer.h"

int main(void)
{
  MKS_TFT28_Init();

  GUI_Clear(0x0000);
  GUI_SetColor(GREEN);
  GUI_FillRect(50, 50, 150, 150);

  for (;;);
}
