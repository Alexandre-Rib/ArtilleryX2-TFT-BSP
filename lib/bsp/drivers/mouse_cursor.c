/**
 * @file    mouse_cursor.c
 * @brief   Software mouse cursor — save-under approach, 12×12 red/white arrow
 * @version 1.0
 * @date    Created: 2026-06-02
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Cursor shape: right-triangle arrow, hotspot at top-left (0,0).
 *  White = 1-pixel border, Red = fill, transparent = save-under background.
 *
 *  Col indices: 0 = leftmost.  Bit N in the mask = column N.
 *
 *  Row  0: W . . . . . . . . . . .   (tip — only 1 pixel)
 *  Row  1: W R W . . . . . . . . .
 *  Row  2: W R R W . . . . . . . .
 *  ...
 *  Row 10: W R R R R R R R R R R W
 *  Row 11: W W W W W W W W W W W W   (bottom border)
 */

#include "mouse_cursor.h"
#include "LCD_Init.h"
#include "LCD_Colors.h"
#include "mks_tft28.h"

#define CUR_W  MOUSE_CURSOR_W   // 12
#define CUR_H  MOUSE_CURSOR_H   // 12

#define C_WHITE  0xFFFFu
#define C_RED    0xF800u   // RGB565 pure red

// Bitmasks per row (bit N = column N)
static const uint16_t k_white[CUR_H] = {
    0x001u, // row  0: W at col 0
    0x005u, // row  1: W at 0,2
    0x009u, // row  2: W at 0,3
    0x011u, // row  3: W at 0,4
    0x021u, // row  4: W at 0,5
    0x041u, // row  5: W at 0,6
    0x081u, // row  6: W at 0,7
    0x101u, // row  7: W at 0,8
    0x201u, // row  8: W at 0,9
    0x401u, // row  9: W at 0,10
    0x801u, // row 10: W at 0,11
    0xFFFu, // row 11: all 12 white
};
static const uint16_t k_red[CUR_H] = {
    0x000u,
    0x002u, // col 1
    0x006u, // cols 1-2
    0x00Eu, // cols 1-3
    0x01Eu, // cols 1-4
    0x03Eu, // cols 1-5
    0x07Eu, // cols 1-6
    0x0FEu, // cols 1-7
    0x1FEu, // cols 1-8
    0x3FEu, // cols 1-9
    0x7FEu, // cols 1-10
    0x000u,
};

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static int16_t  s_cx = 0;
static int16_t  s_cy = 0;
static bool     s_visible = false;

// Save-under region (may be smaller than CUR_W×CUR_H at screen edges)
static int16_t  s_sx = 0, s_sy = 0;  // top-left of saved region on screen
static uint16_t s_sw = 0, s_sh = 0;  // dimensions of saved region
static uint16_t s_bg[CUR_W * CUR_H]; // background buffer (max CUR_W*CUR_H pixels)

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void compute_clip(int16_t cx, int16_t cy,
                         int16_t *sx, int16_t *sy,
                         uint16_t *sw, uint16_t *sh)
{
    int16_t bx0 = cx;
    int16_t by0 = cy;
    int16_t bx1 = cx + CUR_W;   // exclusive
    int16_t by1 = cy + CUR_H;   // exclusive

    int16_t lx = (bx0 < 0) ? 0 : bx0;
    int16_t ly = (by0 < 0) ? 0 : by0;
    int16_t rx = (bx1 > LCD_WIDTH)  ? (int16_t)LCD_WIDTH  : bx1;
    int16_t ry = (by1 > LCD_HEIGHT) ? (int16_t)LCD_HEIGHT : by1;

    *sx = lx;
    *sy = ly;
    *sw = (rx > lx) ? (uint16_t)(rx - lx) : 0u;
    *sh = (ry > ly) ? (uint16_t)(ry - ly) : 0u;
}

static void cursor_save_and_draw(void)
{
    compute_clip(s_cx, s_cy, &s_sx, &s_sy, &s_sw, &s_sh);
    if (s_sw == 0 || s_sh == 0) return;

    // Save background
    LCD_ReadPixels(s_sx, s_sy, s_sw, s_sh, s_bg);

    // Cursor bitmap offset within the visible region
    int16_t boff_x = s_sx - s_cx;   // >= 0
    int16_t boff_y = s_sy - s_cy;   // >= 0

    // Draw row by row (single SetWindow per row = minimal overhead)
    for (uint16_t row = 0; row < s_sh; row++) {
        uint16_t brow    = (uint16_t)((int16_t)row + boff_y);
        uint16_t white_m = k_white[brow];
        uint16_t red_m   = k_red[brow];

        LCD_SetWindow((uint16_t)s_sx,            (uint16_t)(s_sy + (int16_t)row),
                      (uint16_t)(s_sx + (int16_t)s_sw - 1), (uint16_t)(s_sy + (int16_t)row));

        for (uint16_t col = 0; col < s_sw; col++) {
            uint16_t bcol = (uint16_t)((int16_t)col + boff_x);
            uint16_t bit  = 1u << bcol;
            uint16_t color;
            if      (white_m & bit) color = C_WHITE;
            else if (red_m   & bit) color = C_RED;
            else                    color = s_bg[row * s_sw + col];
            LCD_WR_16BITS_DATA(color);
        }
    }
}

static void cursor_restore(void)
{
    if (s_sw == 0 || s_sh == 0) return;
    LCD_SetWindow((uint16_t)s_sx,            (uint16_t)s_sy,
                  (uint16_t)(s_sx + (int16_t)s_sw - 1),
                  (uint16_t)(s_sy + (int16_t)s_sh - 1));
    uint32_t n = (uint32_t)s_sw * s_sh;
    for (uint32_t i = 0; i < n; i++)
        LCD_WR_16BITS_DATA(s_bg[i]);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void MouseCursor_Init(void)
{
    s_cx      = LCD_WIDTH  / 2;
    s_cy      = LCD_HEIGHT / 2;
    s_visible = false;
    s_sw      = 0;
    s_sh      = 0;
}

void MouseCursor_MoveDelta(int8_t dx, int8_t dy)
{
    if (dx == 0 && dy == 0) return;
    int32_t nx = (int32_t)s_cx + dx;
    int32_t ny = (int32_t)s_cy + dy;
    if (nx < 0)          nx = 0;
    if (nx >= LCD_WIDTH)  nx = LCD_WIDTH  - 1;
    if (ny < 0)          ny = 0;
    if (ny >= LCD_HEIGHT) ny = LCD_HEIGHT - 1;
    s_cx = (int16_t)nx;
    s_cy = (int16_t)ny;
}

void MouseCursor_SetPos(int16_t x, int16_t y)
{
    if (x < 0)          x = 0;
    if (x >= LCD_WIDTH)  x = LCD_WIDTH  - 1;
    if (y < 0)          y = 0;
    if (y >= LCD_HEIGHT) y = LCD_HEIGHT - 1;
    s_cx = x;
    s_cy = y;
}

void MouseCursor_Show(void)
{
    if (s_visible) return;
    cursor_save_and_draw();
    s_visible = true;
}

void MouseCursor_Hide(void)
{
    if (!s_visible) return;
    cursor_restore();
    s_visible = false;
}

void MouseCursor_Invalidate(void)
{
    s_visible = false;
    s_sw      = 0;
    s_sh      = 0;
}

bool MouseCursor_IsVisible(void)
{
    return s_visible;
}

void MouseCursor_GetPos(int16_t *x, int16_t *y)
{
    if (x) *x = s_cx;
    if (y) *y = s_cy;
}
