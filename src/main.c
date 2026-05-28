#include "main.h"
#include "includes.h"
#include "LCD_Colors.h"

int main(void){


  SystemClockInit();
  SCB->VTOR = VECT_TAB_FLASH;

  HW_Init();

  // Test : carré vert pour confirmer qu'on contrôle le système
  GUI_Clear(0x0000);
  GUI_SetColor(BLUE);
  GUI_FillRect(50, 50, 150, 150);

  for (;;);
}
