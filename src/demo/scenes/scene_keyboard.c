/**
 * @file    scene_keyboard.c
 * @brief   Scene: live USB HID keyboard display
 * @version 3.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Layout (320x240):
 *    y=  0.. 19 :  title bar "KEYBOARD"                     (20 px)
 *    y= 20.. 75 :  live zone — 3 columns KEYCODE / MOD / CHAR (56 px)
 *    y= 76.. 77 :  horizontal separator                      ( 2 px)
 *    y= 78.. 91 :  log column headers CODE / MOD / CHAR      (14 px)
 *    y= 92..203 :  7 log rows x 16 px                       (112 px)
 *    y=204..213 :  gap (black)                               (10 px)
 *    y=214..239 :  BACK footer button                        (26 px)
 *
 *  All text rendered with the embedded pixel font (no W25Q64 font required).
 *  Live zone: refreshed on every key-state change.
 *  Log: one entry per Keyboard_HasNewKey() event; oldest row scrolls out when full.
 *  BACK: ESC key (propagated to demo_app) or touch tap in the footer area.
 */

#include "scene_keyboard.h"
#include "demo_app.h"
#include "font_embedded.h"
#include "ui_nav.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "keyboard.h"
#include "my_misc.h"
#include "mks_tft28.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
#define KBD_TITLE_H    20
#define KBD_LIVE_H     56
#define KBD_SEP_H       2
#define KBD_LHDR_H     14
#define KBD_ROW_H      16
#define KBD_LOG_ROWS    7
#define KBD_FOOTER_H   26

#define KBD_TITLE_Y1   KBD_TITLE_H
#define KBD_LIVE_Y0    KBD_TITLE_Y1
#define KBD_LIVE_Y1    (KBD_LIVE_Y0 + KBD_LIVE_H)
#define KBD_SEP_Y      KBD_LIVE_Y1
#define KBD_LHDR_Y0    (KBD_SEP_Y  + KBD_SEP_H)
#define KBD_LHDR_Y1    (KBD_LHDR_Y0 + KBD_LHDR_H)
#define KBD_LOG_Y0     KBD_LHDR_Y1
#define KBD_FOOTER_Y0  (LCD_HEIGHT - KBD_FOOTER_H)

// Label sub-row (scale=1 → 8 px tall)
#define KBD_LBL_Y0  (KBD_LIVE_Y0 + 3)
#define KBD_LBL_Y1  (KBD_LBL_Y0 + 8)
// Value sub-row (scale=2 → 16 px tall)
#define KBD_VAL_Y0  (KBD_LIVE_Y0 + 16)
#define KBD_VAL_Y1  (KBD_VAL_Y0  + 16)

// 3 equal columns of 106 px, 1-px separators at x=106 and x=213
#define KBD_C0_X0   0
#define KBD_C0_X1   106
#define KBD_C1_X0   107
#define KBD_C1_X1   213
#define KBD_C2_X0   214
#define KBD_C2_X1   LCD_WIDTH

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------
#define COLOR_TITLE_BG   0x000Fu   // very dark blue
#define COLOR_LIVE_BG    0x0841u   // slightly lighter
#define COLOR_SEP        0x4228u
#define COLOR_LHDR_BG    0x2104u
#define COLOR_LOG_BG0    0x0821u
#define COLOR_LOG_BG1    0x0000u
#define COLOR_BACK_BG    0x8800u   // dark red for back button
#define COLOR_TITLE_FG   0xFFFFu
#define COLOR_HEADER     0x8410u   // muted grey
#define COLOR_CODE       0x07FFu   // cyan
#define COLOR_MOD        0xFFE0u   // yellow
#define COLOR_CHAR       0x07E0u   // green
#define COLOR_BACK_FG    0xFFFFu

// ---------------------------------------------------------------------------
// Log state
// ---------------------------------------------------------------------------
typedef struct {
    uint8_t keycode;
    uint8_t modifiers;
    char    character;
} KeyLogEntry_t;

static KeyLogEntry_t log_buffer[KBD_LOG_ROWS];
static uint8_t       log_count;
static uint8_t       prev_keycode;
static uint8_t       prev_modifiers;

// ---------------------------------------------------------------------------
// String helpers
// ---------------------------------------------------------------------------
static void fmt_hex(uint8_t v, char buf[5])
{
    buf[0] = '0'; buf[1] = 'x';
    uint8_2_string(v, (uint8_t *)(buf + 2));
    buf[4] = '\0';
}

static void fmt_char(char ch, char buf[4])
{
    if      (ch ==  0 ) { buf[0]='-'; buf[1]='-'; buf[2]='-'; buf[3]='\0'; }
    else if (ch == ' ') { buf[0]='S'; buf[1]='P'; buf[2]='C'; buf[3]='\0'; }
    else if (ch =='\n') { buf[0]='R'; buf[1]='E'; buf[2]='T'; buf[3]='\0'; }
    else if (ch =='\b') { buf[0]='B'; buf[1]='S'; buf[2]='P'; buf[3]='\0'; }
    else if (ch =='\t') { buf[0]='T'; buf[1]='A'; buf[2]='B'; buf[3]='\0'; }
    else if ((uint8_t)ch >= 0x20u && (uint8_t)ch < 0x7Fu) { buf[0]=ch; buf[1]='\0'; }
    else                { buf[0]='-'; buf[1]='-'; buf[2]='-'; buf[3]='\0'; }
}

// ---------------------------------------------------------------------------
// Draw helpers — all text via embedded font, no W25Q64 font required
// ---------------------------------------------------------------------------
static void draw_back_icon(int16_t cx, int16_t cy)
{
    GUI_SetColor(COLOR_BACK_FG);
    GUI_DrawLine((uint16_t)(cx+7), (uint16_t)(cy-7), (uint16_t)(cx-4), (uint16_t)cy);
    GUI_DrawLine((uint16_t)(cx-4), (uint16_t)cy,     (uint16_t)(cx+7), (uint16_t)(cy+7));
    GUI_DrawLine((uint16_t)(cx+8), (uint16_t)(cy-7), (uint16_t)(cx-3), (uint16_t)cy);
    GUI_DrawLine((uint16_t)(cx-3), (uint16_t)cy,     (uint16_t)(cx+8), (uint16_t)(cy+7));
}

static void draw_footer(void)
{
    GUI_FillRectColor(0, KBD_FOOTER_Y0, LCD_WIDTH, LCD_HEIGHT, COLOR_BACK_BG);
    draw_back_icon(24, KBD_FOOTER_Y0 + KBD_FOOTER_H / 2);
    Font_DrawStringCentered(48, KBD_FOOTER_Y0, LCD_WIDTH, LCD_HEIGHT,
                             "BACK", 1, COLOR_BACK_FG);
}

static void draw_log_row(uint8_t i)
{
    int16_t  y0 = (int16_t)(KBD_LOG_Y0 + i * KBD_ROW_H);
    int16_t  y1 = y0 + KBD_ROW_H;
    uint16_t bg = (i % 2u == 0u) ? COLOR_LOG_BG0 : COLOR_LOG_BG1;
    char     buf[8];

    GUI_FillRectColor(0, (uint16_t)y0, LCD_WIDTH, (uint16_t)y1, bg);

    fmt_hex(log_buffer[i].keycode, buf);
    Font_DrawStringCentered(KBD_C0_X0+2, y0, KBD_C0_X1-2, y1, buf, 1, COLOR_CODE);

    fmt_hex(log_buffer[i].modifiers, buf);
    Font_DrawStringCentered(KBD_C1_X0+2, y0, KBD_C1_X1-2, y1, buf, 1, COLOR_MOD);

    fmt_char(log_buffer[i].character, buf);
    Font_DrawStringCentered(KBD_C2_X0+2, y0, KBD_C2_X1-2, y1, buf, 1, COLOR_CHAR);
}

static void draw_log_all(void)
{
    for (uint8_t i = 0; i < KBD_LOG_ROWS; i++) {
        if (i < log_count) {
            draw_log_row(i);
        } else {
            int16_t  y0 = (int16_t)(KBD_LOG_Y0 + i * KBD_ROW_H);
            uint16_t bg = (i % 2u == 0u) ? COLOR_LOG_BG0 : COLOR_LOG_BG1;
            GUI_FillRectColor(0, (uint16_t)y0, LCD_WIDTH, (uint16_t)(y0+KBD_ROW_H), bg);
        }
    }
}

static void draw_live(uint8_t keycode, uint8_t modifiers)
{
    char buf[8];

    GUI_FillRectColor(KBD_C0_X0, KBD_VAL_Y0, KBD_C0_X1, KBD_VAL_Y1, COLOR_LIVE_BG);
    GUI_FillRectColor(KBD_C1_X0, KBD_VAL_Y0, KBD_C1_X1, KBD_VAL_Y1, COLOR_LIVE_BG);
    GUI_FillRectColor(KBD_C2_X0, KBD_VAL_Y0, KBD_C2_X1, KBD_VAL_Y1, COLOR_LIVE_BG);

    fmt_hex(keycode, buf);
    Font_DrawStringCentered(KBD_C0_X0+2, KBD_VAL_Y0, KBD_C0_X1-2, KBD_VAL_Y1,
                             buf, 2, keycode ? COLOR_CODE : COLOR_HEADER);

    fmt_hex(modifiers, buf);
    Font_DrawStringCentered(KBD_C1_X0+2, KBD_VAL_Y0, KBD_C1_X1-2, KBD_VAL_Y1,
                             buf, 2, modifiers ? COLOR_MOD : COLOR_HEADER);

    char ch = Keyboard_ToChar(keycode, modifiers);
    fmt_char(ch, buf);
    Font_DrawStringCentered(KBD_C2_X0+2, KBD_VAL_Y0, KBD_C2_X1-2, KBD_VAL_Y1,
                             buf, 2, ch ? COLOR_CHAR : COLOR_HEADER);
}

static void draw_static_layout(void)
{
    GUI_Clear(BLACK);

    // Title
    GUI_FillRectColor(0, 0, LCD_WIDTH, KBD_TITLE_Y1, COLOR_TITLE_BG);
    Font_DrawStringCentered(0, 0, LCD_WIDTH, KBD_TITLE_Y1,
                             "KEYBOARD", 1, COLOR_TITLE_FG);

    // Live zone + column separators
    GUI_FillRectColor(0,          KBD_LIVE_Y0, LCD_WIDTH, KBD_LIVE_Y1, COLOR_LIVE_BG);
    GUI_FillRectColor(KBD_C0_X1,  KBD_LIVE_Y0, KBD_C1_X0, KBD_LIVE_Y1, COLOR_SEP);
    GUI_FillRectColor(KBD_C1_X1,  KBD_LIVE_Y0, KBD_C2_X0, KBD_LIVE_Y1, COLOR_SEP);

    // Column labels (scale=1)
    Font_DrawStringCentered(KBD_C0_X0+2, KBD_LBL_Y0, KBD_C0_X1-2, KBD_LBL_Y1,
                             "KEYCODE", 1, COLOR_HEADER);
    Font_DrawStringCentered(KBD_C1_X0+2, KBD_LBL_Y0, KBD_C1_X1-2, KBD_LBL_Y1,
                             "MOD",     1, COLOR_HEADER);
    Font_DrawStringCentered(KBD_C2_X0+2, KBD_LBL_Y0, KBD_C2_X1-2, KBD_LBL_Y1,
                             "CHAR",    1, COLOR_HEADER);

    // Separator
    GUI_FillRectColor(0, KBD_SEP_Y, LCD_WIDTH, KBD_SEP_Y + KBD_SEP_H, COLOR_SEP);

    // Log header
    GUI_FillRectColor(0, KBD_LHDR_Y0, LCD_WIDTH, KBD_LHDR_Y1, COLOR_LHDR_BG);
    Font_DrawStringCentered(KBD_C0_X0+2, KBD_LHDR_Y0, KBD_C0_X1-2, KBD_LHDR_Y1,
                             "CODE", 1, COLOR_HEADER);
    Font_DrawStringCentered(KBD_C1_X0+2, KBD_LHDR_Y0, KBD_C1_X1-2, KBD_LHDR_Y1,
                             "MOD",  1, COLOR_HEADER);
    Font_DrawStringCentered(KBD_C2_X0+2, KBD_LHDR_Y0, KBD_C2_X1-2, KBD_LHDR_Y1,
                             "CHAR", 1, COLOR_HEADER);

    draw_footer();
    draw_live(0, 0);
    draw_log_all();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void SceneKeyboard_OnEnter(void)
{
    log_count      = 0;
    prev_keycode   = 0;
    prev_modifiers = 0;
    draw_static_layout();
}

bool SceneKeyboard_OnUpdate(uint32_t now_ms, NavigationEvent_t event)
{
    (void)now_ms;

    // Touch in footer area → back
    if (event == NAVIGATION_TOUCH) {
        int16_t tx, ty;
        Navigation_GetTouchPosition(&tx, &ty);
        if (ty >= KBD_FOOTER_Y0) {
            DemoApp_RequestExit();
            return false;
        }
    }

    // Refresh live display on key-state change
    uint8_t keycode   = Keyboard_GetKeycode();
    uint8_t modifiers = Keyboard_GetModifiers();

    if (keycode != prev_keycode || modifiers != prev_modifiers) {
        prev_keycode   = keycode;
        prev_modifiers = modifiers;
        draw_live(keycode, modifiers);
    }

    // Append one log entry per physical key-press
    if (Keyboard_HasNewKey()) {
        char character = Keyboard_ToChar(prev_keycode, prev_modifiers);
        if (log_count < KBD_LOG_ROWS) {
            log_buffer[log_count] = (KeyLogEntry_t){prev_keycode, prev_modifiers, character};
            log_count++;
        } else {
            memmove(log_buffer, log_buffer + 1, (KBD_LOG_ROWS - 1) * sizeof(KeyLogEntry_t));
            log_buffer[KBD_LOG_ROWS - 1] = (KeyLogEntry_t){prev_keycode, prev_modifiers, character};
        }
        draw_log_all();
    }

    return false;  // ESC propagates to demo_app which exits the scene
}

void SceneKeyboard_OnExit(void)
{
}
