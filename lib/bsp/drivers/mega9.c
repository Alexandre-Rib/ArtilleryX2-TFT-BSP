/**
 * @file  mega9.c
 * @brief Driver MEGA9 — manette Sega Mega Drive via Arduino Micro USB HID
 *
 *  L'Arduino Micro se présente au STM32 comme un périphérique HID générique
 *  (Usage Page: Generic Desktop, Usage: Game Pad, subclass=0x00, protocol=0x00).
 *
 *  Format du rapport (Report ID 1, 1 byte de données) :
 *    buff[0] = 0x01  (Report ID)
 *    buff[1] = bitmask boutons (bits MEGA9_BTN_*)
 *
 *  Le stack USB HID du STM32 est étendu dans usbh_hid_core.c pour accepter
 *  les devices HID non-Boot et les interfaces HID sur des devices composites
 *  (CDC + HID, cas de l'Arduino Micro avec Serial).
 */

#include "mega9.h"
#include "keyboard.h"      // Keyboard_IsConnected()
#include "usbh_hid_core.h" // HID_Machine_TypeDef

extern HID_Machine_TypeDef HID_Machine;

#define MEGA9_REPORT_ID   0x01u
#define MEGA9_REPORT_SIZE 2u    // [report_id, buttons]

static uint8_t s_prev_buttons = 0u;
static uint8_t s_new_buttons  = 0u;

void Mega9_Init(void) {}

void Mega9_Process(void)
{
    uint8_t cur = Mega9_GetButtons();
    s_new_buttons |= (cur & ~s_prev_buttons);
    s_prev_buttons  = cur;
}

void Mega9_FlushEdges(void)
{
    s_new_buttons  = 0u;
    s_prev_buttons = Mega9_GetButtons();
}

bool Mega9_IsLinkAlive(void)  { return Joystick_IsConnected(); }
bool Mega9_IsConnected(void)  { return Joystick_IsConnected(); }

uint8_t Mega9_GetButtons(void)
{
    if (!Joystick_IsConnected())
        return 0u;
    if (HID_Machine.buff[0] != MEGA9_REPORT_ID)
        return 0u;
    return HID_Machine.buff[1];
}

uint8_t Mega9_GetNewButtons(void)
{
    uint8_t edges = s_new_buttons;
    s_new_buttons  = 0u;
    return edges;
}
