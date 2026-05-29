/**
 * @file    keyboard.c
 * @brief   USB HID keyboard driver — wraps ST USB Host HID class (MKS TFT28 BSP)
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#include "keyboard.h"
#include "usbh_hid_core.h"
#include "usbh_hid_keybd.h"
#include "usbh_usr.h"       // USB_OTG_Core, USB_Host
#include "usbh_core.h"      // USBH_Process, USBH_Init

// Required stubs for usbh_hid_keybd.c and usbh_hid_mouse.c
#include "usbh_hid_mouse.h"
void USR_KEYBRD_Init(void) {}
void USR_KEYBRD_ProcessData(uint8_t ascii)           { (void)ascii; }
void USR_MOUSE_Init(void)                            {}
void USR_MOUSE_ProcessData(HID_MOUSE_Data_TypeDef *d){ (void)d; }

// Direct access to the HID Boot Protocol report buffer (defined in usbh_hid_core.c).
// buff[0] = modifiers, buff[1] = reserved, buff[2..7] = currently pressed keycodes
extern HID_Machine_TypeDef HID_Machine;

// ---------------------------------------------------------------------------
// Keycode-to-byte lookup tables (ISO-8859-1 for accented characters)
// Size: 0x66 = 102 entries (keycodes 0x00 to 0x65)
// Index: [shift][keycode]
// ---------------------------------------------------------------------------
static const uint8_t qwerty_map[2][0x66] = {
  { // no shift
    0,    0,    0,    0,    'a',  'b',  'c',  'd',   // 0x00-0x07
    'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',   // 0x08-0x0F
    'm',  'n',  'o',  'p',  'q',  'r',  's',  't',   // 0x10-0x17
    'u',  'v',  'w',  'x',  'y',  'z',  '1',  '2',   // 0x18-0x1F
    '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',   // 0x20-0x27
    '\n', 0x1B, '\b', '\t', ' ',  '-',  '=',  '[',   // 0x28-0x2F
    ']',  '\\', 0,    ';',  '\'', '`',  ',',  '.',   // 0x30-0x37
    '/',  0,    0,    0,    0,    0,    0,    0,      // 0x38-0x3F
    0,    0,    0,    0,    0,    0,    0,    0,      // 0x40-0x47
    0,    0,    0,    0,    0,    0,    0,    0,      // 0x48-0x4F
    0,    0,    0,    0,    '/',  '*',  '-',  '+',   // 0x50-0x57
    '\n', '1',  '2',  '3',  '4',  '5',  '6',  '7',   // 0x58-0x5F
    '8',  '9',  '0',  '.',  0,    0,               // 0x60-0x65
  },
  { // shift
    0,    0,    0,    0,    'A',  'B',  'C',  'D',
    'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L',
    'M',  'N',  'O',  'P',  'Q',  'R',  'S',  'T',
    'U',  'V',  'W',  'X',  'Y',  'Z',  '!',  '@',
    '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',
    '\n', 0x1B, '\b', '\t', ' ',  '_',  '+',  '{',
    '}',  '|',  0,    ':',  '"',  '~',  '<',  '>',
    '?',  0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    '/',  '*',  '-',  '+',
    '\n', '1',  '2',  '3',  '4',  '5',  '6',  '7',
    '8',  '9',  '0',  '.',  0,    0,
  }
};

static const uint8_t azerty_map[2][0x66] = {
  { // no shift — AZERTY remapping (ISO-8859-1 for accented chars)
    0,    0,    0,    0,    'q',  'b',  'c',  'd',   // a->q
    'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',
    ',',  'n',  'o',  'p',  'a',  'r',  's',  't',   // m->, q->a
    'u',  'v',  'z',  'x',  'y',  'w',  '&',  0xE9, // w->z, z->w, 2->e acute
    '"',  '\'', '(',  '-',  0xE8, '_',  0xE7, 0xE0, // e grave, c cedilla, a grave
    '\n', 0x1B, '\b', '\t', ' ',  ')',  '=',  '^',
    '$',  '*',  0,    'm',  0xF9, 0xB2, ';',  ':',  // u grave, superscript 2
    '!',  0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    '/',  '*',  '-',  '+',
    '\n', '1',  '2',  '3',  '4',  '5',  '6',  '7',
    '8',  '9',  '0',  '.',  0,    0,
  },
  { // shift
    0,    0,    0,    0,    'Q',  'B',  'C',  'D',
    'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L',
    '?',  'N',  'O',  'P',  'A',  'R',  'S',  'T',
    'U',  'V',  'Z',  'X',  'Y',  'W',  '1',  '2',
    '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',
    '\n', 0x1B, '\b', '\t', ' ',  0xB0, '+',  0,    // degree sign
    0xA3, 0xB5, 0,    'M',  '%',  0,    '.',  '/',  // pound, micro
    '%',  0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    '/',  '*',  '-',  '+',
    '\n', '1',  '2',  '3',  '4',  '5',  '6',  '7',
    '8',  '9',  '0',  '.',  0,    0,
  }
};

static const uint8_t qwertz_map[2][0x66] = {
  { // no shift
    0,    0,    0,    0,    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',  'q',  'r',  's',  't',
    'u',  'v',  'w',  'x',  'z',  'y',  '1',  '2',  // w<->z, z<->y
    '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',
    '\n', 0x1B, '\b', '\t', ' ',  '-',  '=',  0xFC, // u umlaut
    '+',  '#',  0,    0xF6, 0xE4, '^',  ',',  '.',  // o umlaut, a umlaut
    '-',  0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    '/',  '*',  '-',  '+',
    '\n', '1',  '2',  '3',  '4',  '5',  '6',  '7',
    '8',  '9',  '0',  '.',  0,    0,
  },
  { // shift
    0,    0,    0,    0,    'A',  'B',  'C',  'D',
    'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L',
    'M',  'N',  'O',  'P',  'Q',  'R',  'S',  'T',
    'U',  'V',  'W',  'X',  'Z',  'Y',  '!',  '"',
    0xA7, '$',  '%',  '&',  '/',  '(',  ')',  '=',  // section sign
    '\n', 0x1B, '\b', '\t', ' ',  '_',  '`',  0xDC, // U umlaut
    '*',  '\'', 0,    0xD6, 0xC4, 0xB0, ';',  ':',  // O umlaut, A umlaut, degree
    '_',  0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    '/',  '*',  '-',  '+',
    '\n', '1',  '2',  '3',  '4',  '5',  '6',  '7',
    '8',  '9',  '0',  '.',  0,    0,
  }
};

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static KB_LAYOUT current_layout = KB_LAYOUT_QWERTY;
static uint8_t   last_keycode   = 0;
static uint8_t   last_modifiers = 0;
static uint8_t   new_key        = 0;

// ---------------------------------------------------------------------------
// Callback invoked by usbh_hid_keybd.c when a new key-press event arrives
// ---------------------------------------------------------------------------
void Keyboard_ReportCallback(uint8_t modifiers, uint8_t keycode)
{
  if (keycode != last_keycode || modifiers != last_modifiers)
  {
    last_keycode   = keycode;
    last_modifiers = modifiers;
    new_key        = (keycode != KB_KEY_NONE) ? 1 : 0;
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Keyboard_Init(void)
{
  current_layout = KB_LAYOUT_QWERTY;
  last_keycode   = 0;
  last_modifiers = 0;
  new_key        = 0;

  // Initialise USB Host with HID class (incompatible with MSC simultaneously).
  extern USBH_Class_cb_TypeDef HID_cb;
  USBH_Init(&USB_OTG_Core,
             USB_OTG_FS_CORE_ID,
             &USB_Host,
             &HID_cb,
             &USR_cb);
}

void Keyboard_Process(void)
{
  USBH_Process(&USB_OTG_Core, &USB_Host);
}

bool Keyboard_IsConnected(void)
{
  return (uint8_t)HCD_IsDeviceConnected(&USB_OTG_Core);
}

uint8_t Keyboard_GetKeycode(void)
{
  // Read directly from the HID Boot Protocol buffer: reflects current state
  // (key held -> returns keycode; key released -> returns 0)
  return HID_Machine.buff[2];
}

uint8_t Keyboard_GetModifiers(void)
{
  return HID_Machine.buff[0];
}

bool Keyboard_HasNewKey(void)
{
  if (new_key) { new_key = 0; return 1; }
  return 0;
}

void Keyboard_SetLayout(KB_LAYOUT layout)
{
  current_layout = layout;
}

char Keyboard_ToChar(uint8_t keycode, uint8_t modifiers)
{
  if (keycode == 0 || keycode >= 0x66) return 0;

  uint8_t shift = (modifiers & KB_MOD_SHIFT) ? 1 : 0;

  switch (current_layout)
  {
    case KB_LAYOUT_AZERTY: return (char) azerty_map[shift][keycode];
    case KB_LAYOUT_QWERTZ: return (char) qwertz_map[shift][keycode];
    default:               return (char) qwerty_map[shift][keycode];
  }
}
