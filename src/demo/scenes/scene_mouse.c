/**
 * @file    scene_mouse.c
 * @brief   Scene: USB HID mouse diagnostic
 * @version 2.0
 * @date    Created: 2026-06-02
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Layout (320×240):
 *    y=  0.. 19 : title "MOUSE"
 *    y= 20.. 63 : position display (X / Y with large values)
 *    y= 64.. 65 : separator
 *    y= 66..183 : button indicators — LMB (left half) / RMB (right half)
 *    y=184..213 : empty
 *    y=214..239 : BACK footer
 *
 *  Cursor is drawn globally (demo_app main loop) on top of this scene.
 *  Scene reads position via MouseCursor_GetPos() and button state via
 *  Mouse_GetButtons(), refreshing only on change.
 *
 *  Auto-exit: if the mouse is unplugged while this scene is active,
 *  DemoApp_RequestExit() is called immediately.
 */

#include "scene_mouse.h"
#include "demo_app.h"
#include "ui_nav.h"
#include "settings.h"
#include "font_embedded.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "keyboard.h"
#include "mouse_cursor.h"
#include "mks_tft28.h"
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
#define TITLE_H      20
#define POS_H        44   // y=20..63
#define SEP_H         2   // y=64..65
#define BTN_Y0       66
#define BTN_H       118   // y=66..183
#define FOOTER_H     26
#define FOOTER_Y0   (LCD_HEIGHT - FOOTER_H)   // 214

// Button halves
#define LMB_X0   0
#define LMB_X1   (LCD_WIDTH / 2)     // 160
#define RMB_X0   (LCD_WIDTH / 2)     // 160
#define RMB_X1   LCD_WIDTH            // 320

// Colors
#define C_TITLE_BG    0x000Fu
#define C_TITLE_FG    0xFFFFu
#define C_POS_BG      0x0841u
#define C_POS_FG      0xFFFFu
#define C_POS_LBL     0x8410u
#define C_SEP         0x4228u
#define C_BTN_OFF_BG  0x0821u
#define C_BTN_ON_BG   0x07E0u   // bright green
#define C_BTN_OFF_FG  0x8410u
#define C_BTN_ON_FG   0x0000u
#define C_BACK_BG     0x8800u
#define C_BACK_FG     0xFFFFu

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static int16_t s_prev_x   = -1;
static int16_t s_prev_y   = -1;
static uint8_t s_prev_btn = 0xFF;  // force first draw

// ---------------------------------------------------------------------------
// Draw helpers
// ---------------------------------------------------------------------------
static void draw_pos_value(int16_t val, int16_t x0, int16_t x1, int16_t y0, int16_t y1)
{
    // Convert to 3-char decimal string (0..319 or 0..239)
    char buf[6];
    uint16_t v = (val < 0) ? 0u : (uint16_t)val;
    buf[0] = (char)('0' + (v / 100u) % 10u);
    buf[1] = (char)('0' + (v /  10u) % 10u);
    buf[2] = (char)('0' +  v         % 10u);
    buf[3] = '\0';
    Font_DrawStringCentered(x0, y0, x1, y1, buf, 2, C_POS_FG);
}

static void draw_pos(int16_t x, int16_t y)
{
    int16_t py0 = TITLE_H + 4;
    int16_t py1 = TITLE_H + POS_H - 4;

    // Left column: X
    GUI_FillRectColor(0, (uint16_t)(TITLE_H), (uint16_t)(LMB_X1), (uint16_t)(TITLE_H + POS_H), C_POS_BG);
    Font_DrawStringCentered(0, TITLE_H + 2, LMB_X1, TITLE_H + 14, "X", 1, C_POS_LBL);
    draw_pos_value(x, 0, LMB_X1, py0 + 6, py1 + 6);

    // Right column: Y
    GUI_FillRectColor((uint16_t)RMB_X0, (uint16_t)(TITLE_H), LCD_WIDTH, (uint16_t)(TITLE_H + POS_H), C_POS_BG);
    Font_DrawStringCentered(RMB_X0, TITLE_H + 2, LCD_WIDTH, TITLE_H + 14, "Y", 1, C_POS_LBL);
    draw_pos_value(y, RMB_X0, LCD_WIDTH, py0 + 6, py1 + 6);
}

static void draw_button(bool lmb, bool pressed)
{
    int16_t  x0  = lmb ? LMB_X0 : RMB_X0;
    int16_t  x1  = lmb ? LMB_X1 : RMB_X1;
    uint16_t bg  = pressed ? C_BTN_ON_BG  : C_BTN_OFF_BG;
    uint16_t fg  = pressed ? C_BTN_ON_FG  : C_BTN_OFF_FG;
    int16_t  mid = BTN_Y0 + BTN_H / 2;

    GUI_FillRectColor((uint16_t)x0, BTN_Y0, (uint16_t)x1, (uint16_t)(BTN_Y0 + BTN_H), bg);

    // Label (scale 1 at top of each cell)
    Font_DrawStringCentered(x0, BTN_Y0 + 8,  x1, BTN_Y0 + 20,
                             lmb ? "LMB" : "RMB", 1, fg);

    // Large status text
    Font_DrawStringCentered(x0, mid - 8, x1, mid + 8,
                             pressed ? "PRESSED" : "released", 1, fg);
}

static void draw_static_layout(void)
{
    GUI_Clear(BLACK);

    // Title
    GUI_FillRectColor(0, 0, LCD_WIDTH, TITLE_H, C_TITLE_BG);
    Font_DrawStringCentered(0, 0, LCD_WIDTH, TITLE_H, "MOUSE", 1, C_TITLE_FG);

    // Separator
    GUI_FillRectColor(0, (uint16_t)(TITLE_H + POS_H), LCD_WIDTH,
                      (uint16_t)(TITLE_H + POS_H + SEP_H), C_SEP);

    // Vertical separator between LMB / RMB areas
    GUI_FillRectColor((uint16_t)(LMB_X1 - 1), BTN_Y0,
                      (uint16_t)(LMB_X1 + 1), (uint16_t)(BTN_Y0 + BTN_H), C_SEP);

    // BACK footer
    GUI_FillRectColor(0, FOOTER_Y0, LCD_WIDTH, LCD_HEIGHT, C_BACK_BG);
    Font_DrawStringCentered(0, FOOTER_Y0, LCD_WIDTH, LCD_HEIGHT, "BACK", 1, C_BACK_FG);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void SceneMouse_OnEnter(void)
{
    s_prev_x   = -1;
    s_prev_y   = -1;
    s_prev_btn = 0xFFu;   // force first-frame draw

    draw_static_layout();
    draw_pos(0, 0);
    draw_button(true,  false);
    draw_button(false, false);
}

bool SceneMouse_OnUpdate(uint32_t now_ms, NavigationEvent_t event)
{
    (void)now_ms;

    // Auto-exit when mouse is unplugged
    if (!Mouse_IsConnected()) {
        DemoApp_RequestExit();
        return false;
    }

    // Block NAVIGATION_BACK (keyboard/gamepad) from exiting mouse diag when calibration exists.
#ifndef DIAG_BACK_UNLOCKED
    if (event == NAVIGATION_BACK) {
        Settings_t _cfg;
        if (Settings_Load(&_cfg))
            return true;
    }
#endif

    // Touch / LMB click on BACK footer
    if (event == NAVIGATION_TOUCH) {
        int16_t tx, ty;
        Navigation_GetTouchPosition(&tx, &ty);
        if (ty >= FOOTER_Y0) {
            DemoApp_RequestExit();
            return false;
        }
    }

    // Refresh position display
    int16_t cx, cy;
    MouseCursor_GetPos(&cx, &cy);
    if (cx != s_prev_x || cy != s_prev_y) {
        s_prev_x = cx;
        s_prev_y = cy;
        draw_pos(cx, cy);
    }

    // Refresh button states
    uint8_t btn = Mouse_GetButtons();
    if (btn != s_prev_btn) {
        bool lmb = (btn & MOUSE_BTN_LEFT)  != 0u;
        bool rmb = (btn & MOUSE_BTN_RIGHT) != 0u;
        bool prev_lmb = (s_prev_btn & MOUSE_BTN_LEFT)  != 0u;
        bool prev_rmb = (s_prev_btn & MOUSE_BTN_RIGHT) != 0u;

        if (lmb != prev_lmb) draw_button(true,  lmb);
        if (rmb != prev_rmb) draw_button(false, rmb);

        s_prev_btn = btn;
    }

    return false;   // ESC propagated to SceneControllers
}

void SceneMouse_OnExit(void) {}
