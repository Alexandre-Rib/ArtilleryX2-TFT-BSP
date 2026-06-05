/**
 * @file    scene_controllers.c
 * @brief   Scene: sous-menu Controllers (2 tuiles — Keyboard / Joystick)
 */

#include "scene_controllers.h"
#include "scene_keyboard.h"
#include "scene_joystick.h"
#include "demo_app.h"
#include "ui_menu.h"
#include "ui_button.h"
#include "ui_nav.h"
#include "img_draw.h"
#include "res_map.h"
#include "font_embedded.h"
#include "keyboard.h"
#include "mega9.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "mks_tft28.h"
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Layout — 2 boutons centrés dans 320×240
//   BTN_W=96, BTN_H=108, GAP=8
//   total width = 2*96 + 8 = 200  →  offset x = (320-200)/2 = 60
// ---------------------------------------------------------------------------
#define BTN_W      96
#define BTN_H     108
#define BTN_GAP     8
#define BTN_X0     60   // (320 - 2*96 - 8) / 2
#define BTN_X(c)  (BTN_X0 + (c) * (BTN_W + BTN_GAP))
#define BTN_Y0     61   // (240 - 14 - 26 - 108) / 2 + 14

#define TITLE_H    14
#define FOOTER_H   26
#define FOOTER_Y0  (LCD_HEIGHT - FOOTER_H)

#define C_TITLE_BG  0x000Fu
#define C_TITLE_FG  0xFFFFu
#define C_BACK_BG   0x8800u
#define C_BACK_FG   0xFFFFu

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
typedef enum { STATE_MENU, STATE_KBD, STATE_JOY } CtrlState_t;

static CtrlState_t s_state;
static bool        s_prev_kbd;
static bool        s_prev_joy;

// ---------------------------------------------------------------------------
// Button drawing
// ---------------------------------------------------------------------------
static void ctrl_draw_btn(uint8_t idx, const char *img_name,
                           bool available, bool focused)
{
    int16_t bx = (int16_t)BTN_X(idx);
    int16_t by = BTN_Y0;

    uint16_t bg = available ? 0x2104u : 0x18C3u;
    GUI_FillRectColor((uint16_t)bx, (uint16_t)by,
                      (uint16_t)(bx + BTN_W), (uint16_t)(by + BTN_H), bg);

    if (focused && available) {
        uint16_t c = 0x07FFu;
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)by,           (uint16_t)(bx+BTN_W),   (uint16_t)(by+2),      c);
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)(by+BTN_H-2), (uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)by,           (uint16_t)(bx+2),        (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)(bx+BTN_W-2), (uint16_t)by,           (uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
    } else if (!available) {
        uint16_t c = 0x2104u;
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)by,           (uint16_t)(bx+BTN_W),   (uint16_t)(by+1),      c);
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)(by+BTN_H-1), (uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)by,           (uint16_t)(bx+1),        (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)(bx+BTN_W-1), (uint16_t)by,           (uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
    } else {
        uint16_t c = 0x4228u;
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)by,           (uint16_t)(bx+BTN_W),   (uint16_t)(by+1),      c);
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)(by+BTN_H-1), (uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)by,           (uint16_t)(bx+1),        (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)(bx+BTN_W-1), (uint16_t)by,           (uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
    }

    int16_t icon_x = bx + (BTN_W - (int16_t)RES_IMG_W) / 2;
    int16_t icon_y = by + 4;
    ImgDraw_ByName(img_name, icon_x, icon_y);

    int16_t lbl_y0 = icon_y + (int16_t)RES_IMG_H + 2;
    uint16_t fg = available ? WHITE : 0x528Au;
    Font_DrawStringCentered(bx, lbl_y0, bx + BTN_W, by + BTN_H,
                             (idx == 0) ? "KEYBOARD" : "JOYSTICK", 1, fg);
}

static void draw_kbd(bool f)
{
    bool conn = Keyboard_IsConnected();
    ctrl_draw_btn(0, conn ? "keyboard" : "unplugged_device", conn, f);
}
static void draw_joy(bool f)
{
    bool conn = Mega9_IsConnected();
    ctrl_draw_btn(1, conn ? "joystick" : "unplugged_device", conn, f);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------
static void open_keyboard(void)
{
    if (!Keyboard_IsConnected()) return;
    SceneKeyboard_OnEnter();
    s_state = STATE_KBD;
}
static void open_joystick(void)
{
    if (!Mega9_IsConnected()) return;
    SceneJoystick_OnEnter();
    s_state = STATE_JOY;
}

// ---------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------
static MenuItem_t s_items[2] = {
    { { BTN_X(0), BTN_Y0, BTN_W, BTN_H, NULL, BUTTON_NORMAL }, open_keyboard, draw_kbd },
    { { BTN_X(1), BTN_Y0, BTN_W, BTN_H, NULL, BUTTON_NORMAL }, open_joystick, draw_joy },
};

static Menu_t s_menu = {
    .items   = s_items,
    .count   = 2,
    .cols    = 2,
    .focused = 0,
    .parent  = NULL,
};

static void sync_states(bool kbd, bool joy)
{
    bool avail[2] = { kbd, joy };
    for (uint8_t i = 0; i < 2u; i++) {
        if (s_items[i].button.state == BUTTON_FOCUSED) continue;
        s_items[i].button.state = avail[i] ? BUTTON_NORMAL : BUTTON_DISABLED;
    }
    uint8_t f = s_menu.focused;
    if (!avail[f]) {
        for (uint8_t i = 0; i < 2u; i++) {
            if (avail[i]) {
                s_items[f].button.state = BUTTON_DISABLED;
                s_items[i].button.state = BUTTON_FOCUSED;
                s_menu.focused = i;
                return;
            }
        }
        s_items[f].button.state = BUTTON_DISABLED;
    }
}

static void draw_menu(void)
{
    GUI_Clear(BLACK);
    GUI_FillRectColor(0, 0, LCD_WIDTH, TITLE_H, C_TITLE_BG);
    Font_DrawStringCentered(0, 0, LCD_WIDTH, TITLE_H, "CONTROLLERS", 1, C_TITLE_FG);
    Menu_Draw(&s_menu);

    GUI_FillRectColor(0, 174, LCD_WIDTH, 200, BLACK);
    Font_DrawStringCentered(0, 178, LCD_WIDTH, 196, USB_GetDiagStr(), 1, 0xFFFFu);

    GUI_FillRectColor(0, FOOTER_Y0, LCD_WIDTH, LCD_HEIGHT, C_BACK_BG);
    Font_DrawStringCentered(0, FOOTER_Y0, LCD_WIDTH, LCD_HEIGHT, "BACK", 1, C_BACK_FG);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void SceneControllers_OnEnter(void)
{
    s_state    = STATE_MENU;
    s_prev_kbd = Keyboard_IsConnected();
    s_prev_joy = Mega9_IsConnected();

    s_items[0].button.state = s_prev_kbd ? BUTTON_NORMAL : BUTTON_DISABLED;
    s_items[1].button.state = s_prev_joy ? BUTTON_NORMAL : BUTTON_DISABLED;

    s_menu.focused = 0;
    for (uint8_t i = 0; i < 2u; i++) {
        if (s_items[i].button.state == BUTTON_NORMAL) {
            s_items[i].button.state = BUTTON_FOCUSED;
            s_menu.focused = i;
            break;
        }
    }
    draw_menu();
}

bool SceneControllers_OnUpdate(uint32_t now_ms, NavigationEvent_t event)
{
    if (s_state == STATE_MENU) {
        if (event == NAVIGATION_TOUCH) {
            int16_t tx, ty;
            Navigation_GetTouchPosition(&tx, &ty);
            if (ty >= FOOTER_Y0) {
                DemoApp_RequestExit();
                return false;
            }
        }

        bool kbd = Keyboard_IsConnected();
        bool joy = Mega9_IsConnected();

        if (kbd != s_prev_kbd || joy != s_prev_joy) {
            s_prev_kbd = kbd;
            s_prev_joy = joy;
            sync_states(kbd, joy);
            draw_menu();
        } else {
            static uint32_t s_diag_ms = 0;
            if (now_ms - s_diag_ms >= 500u) {
                s_diag_ms = now_ms;
                GUI_FillRectColor(0, 174, LCD_WIDTH, 200, BLACK);
                Font_DrawStringCentered(0, 178, LCD_WIDTH, 196,
                                        USB_GetDiagStr(), 1, 0xFFFFu);
            }
        }

        Menu_HandleEvent(&s_menu, event);
        return false;
    }

    bool consumed = false;
    switch (s_state) {
    case STATE_KBD: consumed = SceneKeyboard_OnUpdate(now_ms, event); break;
    case STATE_JOY: consumed = SceneJoystick_OnUpdate(now_ms, event); break;
    default: break;
    }

    if (DemoApp_IsExitRequested() || (!consumed && event == NAVIGATION_BACK)) {
        DemoApp_CancelExit();
        if (s_state == STATE_KBD) SceneKeyboard_OnExit();
        else                      SceneJoystick_OnExit();

        s_prev_kbd = Keyboard_IsConnected();
        s_prev_joy = Mega9_IsConnected();
        sync_states(s_prev_kbd, s_prev_joy);
        s_state = STATE_MENU;
        draw_menu();
        return true;
    }
    return true;
}

void SceneControllers_OnExit(void)
{
    if (s_state == STATE_KBD) SceneKeyboard_OnExit();
    else if (s_state == STATE_JOY) SceneJoystick_OnExit();
    s_state = STATE_MENU;
}
