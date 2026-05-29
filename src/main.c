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

  // Test buzzer : gamme Do-Ré-Mi à volume 20
  Buzzer_PlayTone(262, 20, 200);   // Do
  Buzzer_PlayTone(294, 20, 200);   // Ré
  Buzzer_PlayTone(330, 20, 200);   // Mi
  Buzzer_PlayTone(349, 20, 200);   // Fa
  Buzzer_PlayTone(392, 20, 200);   // Sol
  Buzzer_PlayTone(440, 20, 200);   // La
  Buzzer_PlayTone(494, 20, 200);   // Si
  Buzzer_PlayTone(523, 20, 300);   // Do (octave)

  for (;;);
}
