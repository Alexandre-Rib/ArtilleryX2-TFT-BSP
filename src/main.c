#include "mks_tft28.h"
#include "GUI.h"
#include "LCD_Colors.h"

int main(void)
{
  MKS_TFT28_Init();

  GUI_Clear(0x0000);
  GUI_SetColor(GREEN);
  GUI_FillRect(50, 50, 150, 150);

  for (;;);
}
