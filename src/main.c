#include "HW_Init.h"
#include "GUI.h"
#include "LCD_Colors.h"

int main(void)
{
  HW_Init();

  GUI_Clear(0x0000);
  GUI_SetColor(RED);
  GUI_FillRect(50, 50, 150, 150);

  for (;;);
}
