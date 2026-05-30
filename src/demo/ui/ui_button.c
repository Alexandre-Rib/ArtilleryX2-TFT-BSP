/**
 * @file    ui_button.c
 * @brief   Button widget — draw, hit-test, keyboard focus aura
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
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
#define COLOR_BODY_NORMAL   0x2104u   // dark navy
#define COLOR_BODY_PRESSED  0x4A69u   // lighter blue-gray
#define COLOR_BODY_DISABLED 0x18C3u   // dark gray
#define COLOR_LABEL         0xFFFFu   // white
#define COLOR_LABEL_DISABLED 0x528Au  // muted gray
#define COLOR_BORDER_NORMAL 0x4228u   // subtle dark border
#define COLOR_FOCUS_RING    0x07FFu   // cyan focus aura

#define FOCUS_RING_THICKNESS  2   // pixels
#define NORMAL_BORDER_THICKNESS 1 // pixels

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/**
 * @brief  Draw a rectangular border of given thickness and color.
 *
 * @param[in] x          Left edge.
 * @param[in] y          Top edge.
 * @param[in] width      Total button width.
 * @param[in] height     Total button height.
 * @param[in] thickness  Border width in pixels.
 * @param[in] color      RGB565 color.
 */
static void draw_border(int16_t x, int16_t y, int16_t width, int16_t height,
                        int16_t thickness, uint16_t color)
{
    GUI_FillRectColor(x,                       y,
                      x + width,               y + thickness,               color); // top
    GUI_FillRectColor(x,                       y + height - thickness,
                      x + width,               y + height,                  color); // bottom
    GUI_FillRectColor(x,                       y,
                      x + thickness,           y + height,                  color); // left
    GUI_FillRectColor(x + width - thickness,   y,
                      x + width,               y + height,                  color); // right
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Button_Draw(const Button_t *button)
{
    int16_t x = button->x;
    int16_t y = button->y;
    int16_t w = button->w;
    int16_t h = button->h;

    uint16_t body_color;
    uint16_t label_color;
    int16_t  border_thickness;
    uint16_t border_color;

    switch (button->state) {
        case BUTTON_FOCUSED:
            body_color        = COLOR_BODY_NORMAL;
            label_color       = COLOR_LABEL;
            border_thickness  = FOCUS_RING_THICKNESS;
            border_color      = COLOR_FOCUS_RING;
            break;
        case BUTTON_PRESSED:
            body_color        = COLOR_BODY_PRESSED;
            label_color       = COLOR_LABEL;
            border_thickness  = FOCUS_RING_THICKNESS;
            border_color      = COLOR_FOCUS_RING;
            break;
        case BUTTON_DISABLED:
            body_color        = COLOR_BODY_DISABLED;
            label_color       = COLOR_LABEL_DISABLED;
            border_thickness  = 0;
            border_color      = 0;
            break;
        default: // BUTTON_NORMAL
            body_color        = COLOR_BODY_NORMAL;
            label_color       = COLOR_LABEL;
            border_thickness  = NORMAL_BORDER_THICKNESS;
            border_color      = COLOR_BORDER_NORMAL;
            break;
    }

    // 1 — fill body
    GUI_FillRectColor(x, y, x + w, y + h, body_color);

    // 2 — border or focus ring
    if (border_thickness > 0)
        draw_border(x, y, w, h, border_thickness, border_color);

    // 3 — label centered inside the inner area (inset from the border)
    if (button->label && button->label[0] != '\0') {
        int16_t padding = border_thickness + 3;
        setFontSize(FONT_SIZE_NORMAL);
        GUI_SetTextMode(GUI_TEXTMODE_TRANS);
        GUI_SetColor(label_color);
        _GUI_DispStringInRect(x + padding,     y + padding,
                              x + w - padding, y + h - padding,
                              (const uint8_t *)button->label);
    }
}

bool Button_HitTest(const Button_t *button, int16_t x, int16_t y)
{
    return (x >= button->x && x < button->x + button->w &&
            y >= button->y && y < button->y + button->h);
}
