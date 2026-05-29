/**
 * @file    ui_button.c
 * @brief   Button widget — draw, hit-test, keyboard focus aura
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#include "ui_button.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "font_render.h"
#include "mks_tft28.h"

// ---------------------------------------------------------------------------
// Color palette (RGB565)
// ---------------------------------------------------------------------------
#define COL_BTN_BG        0x2104u   // dark navy — normal body
#define COL_BTN_BG_PRESS  0x4A69u   // lighter blue-gray on press
#define COL_BTN_BG_DIS    0x18C3u   // dark gray — disabled body
#define COL_TEXT          0xFFFFu   // white
#define COL_TEXT_DIS      0x528Au   // muted gray for disabled labels
#define COL_BORDER        0x4228u   // subtle dark border for normal state
#define COL_AURA          0x07FFu   // cyan — keyboard focus ring
#define COL_AURA_BORDER   2         // focus ring thickness in pixels
#define COL_BORDER_NORMAL 1         // normal border thickness

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void draw_border(int16_t x, int16_t y, int16_t w, int16_t h,
                        int16_t thickness, uint16_t color)
{
    GUI_FillRectColor(x,             y,             x + w,         y + thickness, color); // top
    GUI_FillRectColor(x,             y + h - thickness, x + w,     y + h,         color); // bottom
    GUI_FillRectColor(x,             y,             x + thickness, y + h,         color); // left
    GUI_FillRectColor(x + w - thickness, y,         x + w,         y + h,         color); // right
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Button_Draw(const Button_t *btn)
{
    int16_t x = btn->x;
    int16_t y = btn->y;
    int16_t w = btn->w;
    int16_t h = btn->h;

    uint16_t bg_color;
    uint16_t text_color;
    int16_t  border_px;
    uint16_t border_color;

    switch (btn->state) {
        case BTN_FOCUSED:
            bg_color     = COL_BTN_BG;
            text_color   = COL_TEXT;
            border_px    = COL_AURA_BORDER;
            border_color = COL_AURA;
            break;
        case BTN_PRESSED:
            bg_color     = COL_BTN_BG_PRESS;
            text_color   = COL_TEXT;
            border_px    = COL_AURA_BORDER;
            border_color = COL_AURA;
            break;
        case BTN_DISABLED:
            bg_color     = COL_BTN_BG_DIS;
            text_color   = COL_TEXT_DIS;
            border_px    = 0;
            border_color = 0;
            break;
        default: // BTN_NORMAL
            bg_color     = COL_BTN_BG;
            text_color   = COL_TEXT;
            border_px    = COL_BORDER_NORMAL;
            border_color = COL_BORDER;
            break;
    }

    // 1 — fill body
    GUI_FillRectColor(x, y, x + w, y + h, bg_color);

    // 2 — border / aura
    if (border_px > 0)
        draw_border(x, y, w, h, border_px, border_color);

    // 3 — label centered in the inner rect (inset from border)
    if (btn->label && btn->label[0] != '\0') {
        int16_t pad = border_px + 3;   // 3px text padding from border
        setFontSize(FONT_SIZE_NORMAL);
        GUI_SetTextMode(GUI_TEXTMODE_TRANS);
        GUI_SetColor(text_color);
        _GUI_DispStringInRect(x + pad, y + pad,
                              x + w - pad, y + h - pad,
                              (const uint8_t *)btn->label);
    }
}

bool Button_HitTest(const Button_t *btn, int16_t tx, int16_t ty)
{
    return (tx >= btn->x && tx < btn->x + btn->w &&
            ty >= btn->y && ty < btn->y + btn->h);
}
