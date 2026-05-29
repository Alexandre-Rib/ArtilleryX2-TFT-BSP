#include "demo_keyboard.h"
#include "keyboard.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "os_timer.h"

// ---------------------------------------------------------------------------
// Paramètres
// ---------------------------------------------------------------------------
#define SQUARE_W      60
#define SQUARE_H      60
#define TARGET_FPS    30
#define FRAME_MS      (1000u / TARGET_FPS)   // 33 ms

// 150 px/s ÷ 30fps = 5 px/frame
// Le carré traverse l'écran en ~2.1 s — réactif, naturel pour un jeu 2D
#define SPEED_PX_S    150
#define STEP          (SPEED_PX_S / TARGET_FPS)  // = 5

// Marges écran
#define X_MAX  (LCD_WIDTH  - SQUARE_W)
#define Y_MAX  (LCD_HEIGHT - SQUARE_H)

// ---------------------------------------------------------------------------
// Rendu delta — efface les bandes vacantes puis dessine le nouveau rect
// ---------------------------------------------------------------------------
static void render(int16_t old_x, int16_t old_y, int16_t new_x, int16_t new_y)
{
  int16_t dx = new_x - old_x;
  int16_t dy = new_y - old_y;

  // Dessine d'abord le nouveau rect (aucun trou visible)
  GUI_SetColor(RED);
  GUI_FillRect(new_x, new_y, new_x + SQUARE_W, new_y + SQUARE_H);

  // Efface les bandes vacantes
  GUI_SetColor(BLACK);
  if (dx > 0) GUI_FillRect(old_x,           old_y, old_x + dx,       old_y + SQUARE_H);
  else if (dx < 0) GUI_FillRect(new_x + SQUARE_W, old_y, old_x + SQUARE_W, old_y + SQUARE_H);
  if (dy > 0) GUI_FillRect(old_x,           old_y, old_x + SQUARE_W, old_y + dy);
  else if (dy < 0) GUI_FillRect(old_x, new_y + SQUARE_H, old_x + SQUARE_W, old_y + SQUARE_H);
}

// ---------------------------------------------------------------------------
// Boucle principale
// ---------------------------------------------------------------------------
void DemoKeyboard_Run(void)
{
  // Init USB clavier (remplace le MSC)
  Keyboard_Init();

  // Position initiale : centré
  int16_t pos_x = (LCD_WIDTH  - SQUARE_W) / 2;
  int16_t pos_y = (LCD_HEIGHT - SQUARE_H) / 2;

  GUI_Clear(BLACK);
  GUI_SetColor(RED);
  GUI_FillRect(pos_x, pos_y, pos_x + SQUARE_W, pos_y + SQUARE_H);

  uint32_t last_frame = OS_GetTimeMs();

  while (1)
  {
    // USB : pomper aussi vite que possible (énumération HID à 1ms)
    Keyboard_Process();

    // Cadence rendu à 30fps
    if (OS_GetTimeMs() - last_frame < FRAME_MS) continue;
    last_frame = OS_GetTimeMs();

    // Lecture touche courante (mouvement continu si maintenu)
    uint8_t key = Keyboard_GetKeycode();

    int16_t nx = pos_x;
    int16_t ny = pos_y;

    switch (key)
    {
      case KB_KEY_LEFT:  nx -= STEP; break;
      case KB_KEY_RIGHT: nx += STEP; break;
      case KB_KEY_UP:    ny -= STEP; break;
      case KB_KEY_DOWN:  ny += STEP; break;
      default: continue;  // aucune flèche → pas de rendu ce frame
    }

    // Clamp aux bords
    if (nx < 0)      nx = 0;
    if (nx > X_MAX)  nx = X_MAX;
    if (ny < 0)      ny = 0;
    if (ny > Y_MAX)  ny = Y_MAX;

    // Rendu uniquement si la position a changé
    if (nx != pos_x || ny != pos_y)
    {
      render(pos_x, pos_y, nx, ny);
      pos_x = nx;
      pos_y = ny;
    }
  }
}
