#include "main.h"
#include "includes.h"


int main(void)
{
  #ifdef GD32F3XX
    __enable_irq();
  #endif

  SystemClockInit();
  SCB->VTOR = VECT_TAB_FLASH;

  HW_Init();

  #if defined(SERIAL_DEBUG_PORT) && defined(SERIAL_DEBUG_ENABLED)
    dbg_print("Main Startup: Generic debug output is enabled.\n");
  #endif

  // Test : carré vert pour confirmer qu'on contrôle le système
  GUI_Clear(0x0000);
  GUI_SetColor(0x07E0);
  GUI_FillRect(50, 50, 150, 150);

  for (;;);
}
