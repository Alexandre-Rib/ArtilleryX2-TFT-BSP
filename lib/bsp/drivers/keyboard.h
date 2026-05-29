#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Layouts clavier supportés
// ---------------------------------------------------------------------------
typedef enum {
  KB_LAYOUT_QWERTY = 0,   // US/UK
  KB_LAYOUT_AZERTY,       // FR
  KB_LAYOUT_QWERTZ,       // DE/AT/CH
} KB_LAYOUT;

// ---------------------------------------------------------------------------
// Modificateurs (Byte 0 du rapport HID Boot Protocol)
// ---------------------------------------------------------------------------
#define KB_MOD_LCTRL   (1 << 0)
#define KB_MOD_LSHIFT  (1 << 1)
#define KB_MOD_LALT    (1 << 2)
#define KB_MOD_LMETA   (1 << 3)
#define KB_MOD_RCTRL   (1 << 4)
#define KB_MOD_RSHIFT  (1 << 5)
#define KB_MOD_RALT    (1 << 6)
#define KB_MOD_RMETA   (1 << 7)

#define KB_MOD_SHIFT   (KB_MOD_LSHIFT | KB_MOD_RSHIFT)
#define KB_MOD_CTRL    (KB_MOD_LCTRL  | KB_MOD_RCTRL)
#define KB_MOD_ALT     (KB_MOD_LALT   | KB_MOD_RALT)

// ---------------------------------------------------------------------------
// Keycodes HID USB spéciaux (non imprimables)
// ---------------------------------------------------------------------------
#define KB_KEY_NONE        0x00
#define KB_KEY_UP          0x52
#define KB_KEY_DOWN        0x51
#define KB_KEY_LEFT        0x50
#define KB_KEY_RIGHT       0x4F
#define KB_KEY_ENTER       0x28
#define KB_KEY_ESCAPE      0x29
#define KB_KEY_BACKSPACE   0x2A
#define KB_KEY_TAB         0x2B
#define KB_KEY_SPACE       0x2C
#define KB_KEY_F1          0x3A
#define KB_KEY_F2          0x3B
#define KB_KEY_F3          0x3C
#define KB_KEY_F4          0x3D
#define KB_KEY_F5          0x3E
#define KB_KEY_F6          0x3F
#define KB_KEY_F7          0x40
#define KB_KEY_F8          0x41
#define KB_KEY_F9          0x42
#define KB_KEY_F10         0x43
#define KB_KEY_F11         0x44
#define KB_KEY_F12         0x45

// ---------------------------------------------------------------------------
// API publique
// ---------------------------------------------------------------------------

// Initialise la classe HID dans la pile USB Host
void     Keyboard_Init(void);

// Doit être appelé régulièrement depuis la boucle principale
void     Keyboard_Process(void);

// Vrai si un clavier est connecté et énuméré
bool     Keyboard_IsConnected(void);

// Dernier keycode USB reçu (0 si aucune touche)
uint8_t  Keyboard_GetKeycode(void);

// Modificateurs actifs (masque KB_MOD_*)
uint8_t  Keyboard_GetModifiers(void);

// Convertit un keycode USB en caractère ASCII selon le layout actif.
// Retourne 0 si non imprimable.
char     Keyboard_ToChar(uint8_t keycode, uint8_t modifiers);

// Change le layout actif (défaut : QWERTY)
void     Keyboard_SetLayout(KB_LAYOUT layout);

// Vrai si une nouvelle touche a été pressée depuis le dernier appel
bool     Keyboard_HasNewKey(void);

#endif
