/**
 * @file    scene_calib.c
 * @brief   Scene: XPT2046 touch calibration — 7-seg display + W25Q64 persistence
 * @version 1.1
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  No font atlas required: all values rendered with a built-in 7-segment
 *  renderer using GUI_FillRectColor primitives only.
 *
 *  On ESC: if at least one valid touch was recorded, calibration is saved
 *  to W25Q64 (SETTINGS_ADDR) and applied immediately via Nav_SetCalibration.
 */

#include "scene_calib.h"
#include "settings.h"
#include "ui_nav.h"
#include "xpt2046.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "mks_tft28.h"
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Touch channels
// ---------------------------------------------------------------------------
#define CAL_X_CMD  0xD0
#define CAL_Y_CMD  0x90

// ---------------------------------------------------------------------------
// 7-segment renderer
//  Digit cell: W=12 px wide, H=18 px tall, T=2 px thick segments
//  Segment layout:
//       aaa
//      f   b
//      f   b
//       ggg
//      e   c
//      e   c
//       ddd
// ---------------------------------------------------------------------------
#define SEG_W  12
#define SEG_H  18
#define SEG_T   2
#define SEG_M   (SEG_H / 2)   // = 9 (midpoint)
#define SEG_GAP  3             // pixels between digits in a number

// Segment bitmask: bit0=a(top) b(tr) c(br) d(bot) e(bl) f(tl) g(mid)
static const uint8_t SEG7[10] = {
    0x3F, // 0: a b c d e f
    0x06, // 1: b c
    0x5B, // 2: a b d e g
    0x4F, // 3: a b c d g
    0x66, // 4: b c f g
    0x6D, // 5: a c d f g
    0x7D, // 6: a c d e f g
    0x07, // 7: a b c
    0x7F, // 8: a b c d e f g
    0x6F, // 9: a b c d f g
};

static void seg7_draw_digit(int16_t x, int16_t y, uint8_t digit, uint16_t color)
{
    if (digit > 9) return;
    uint8_t m = SEG7[digit];
    uint16_t bg = BLACK;

    // a — top horizontal
    GUI_FillRectColor((uint16_t)(x+SEG_T), (uint16_t)y,
                      (uint16_t)(x+SEG_W-SEG_T), (uint16_t)(y+SEG_T),
                      (m & 0x01) ? color : bg);
    // b — top-right vertical
    GUI_FillRectColor((uint16_t)(x+SEG_W-SEG_T), (uint16_t)y,
                      (uint16_t)(x+SEG_W), (uint16_t)(y+SEG_M),
                      (m & 0x02) ? color : bg);
    // c — bottom-right vertical
    GUI_FillRectColor((uint16_t)(x+SEG_W-SEG_T), (uint16_t)(y+SEG_M),
                      (uint16_t)(x+SEG_W), (uint16_t)(y+SEG_H),
                      (m & 0x04) ? color : bg);
    // d — bottom horizontal
    GUI_FillRectColor((uint16_t)(x+SEG_T), (uint16_t)(y+SEG_H-SEG_T),
                      (uint16_t)(x+SEG_W-SEG_T), (uint16_t)(y+SEG_H),
                      (m & 0x08) ? color : bg);
    // e — bottom-left vertical
    GUI_FillRectColor((uint16_t)x, (uint16_t)(y+SEG_M),
                      (uint16_t)(x+SEG_T), (uint16_t)(y+SEG_H),
                      (m & 0x10) ? color : bg);
    // f — top-left vertical
    GUI_FillRectColor((uint16_t)x, (uint16_t)y,
                      (uint16_t)(x+SEG_T), (uint16_t)(y+SEG_M),
                      (m & 0x20) ? color : bg);
    // g — middle horizontal
    GUI_FillRectColor((uint16_t)(x+SEG_T), (uint16_t)(y+SEG_M-SEG_T/2),
                      (uint16_t)(x+SEG_W-SEG_T), (uint16_t)(y+SEG_M+SEG_T/2),
                      (m & 0x40) ? color : bg);
}

// Draw a 4-digit decimal number (0-9999) at (x,y) with the given color.
// Digits are left-padded with leading zeros.
static void seg7_draw_u16(int16_t x, int16_t y, uint16_t value, uint16_t color)
{
    int16_t cx = x;
    int16_t step = SEG_W + SEG_GAP;
    seg7_draw_digit(cx,          y, (value / 1000) % 10, color); cx += step;
    seg7_draw_digit(cx,          y, (value /  100) % 10, color); cx += step;
    seg7_draw_digit(cx,          y, (value /   10) % 10, color); cx += step;
    seg7_draw_digit(cx,          y,  value         % 10, color);
}

// Width of a 4-digit number in pixels
#define NUM4_W  (4 * SEG_W + 3 * SEG_GAP)   // = 57 px

// ---------------------------------------------------------------------------
// Layout constants (all Y positions from top of screen)
// ---------------------------------------------------------------------------
#define TITLE_H     24
#define ROW_RAW_Y   (TITLE_H + 6)
#define ROW_MIN_Y   (ROW_RAW_Y + SEG_H + 8)
#define ROW_MAX_Y   (ROW_MIN_Y + SEG_H + 8)
#define BAR_X_Y     (ROW_MAX_Y + SEG_H + 8)
#define BAR_Y_Y     (BAR_X_Y + 12)
#define CROSS_Y0    (BAR_Y_Y + 16)           // top of crosshair area
#define FOOTER_Y    (LCD_HEIGHT - 14)

// Column positions: label swatch (8px), gap, X value, gap, Y value
#define COL_LABEL   2
#define COL_X_VAL   18
#define COL_Y_VAL   (COL_X_VAL + NUM4_W + 10)

// Colors
#define COL_BG        0x0841u
#define COL_TITLE_BG  0x2965u
#define COL_RAW       0xFFFFu   // white — live raw values
#define COL_MIN       0x07E0u   // green — minimum accumulated
#define COL_MAX       0xF800u   // red   — maximum accumulated
#define COL_BAR_X     0x07FFu   // cyan  — X ADC bar
#define COL_BAR_Y     0xF81Fu   // magenta — Y ADC bar
#define COL_BAR_BG    0x2104u
#define COL_CROSS     0xFFE0u   // yellow crosshair
#define COL_SAVED     0x07E0u   // green flash on save

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static uint16_t raw_x = 0, raw_y = 0;
static uint16_t min_x = 0xFFFFu, max_x = 0u;
static uint16_t min_y = 0xFFFFu, max_y = 0u;
static int16_t  cross_x = -1, cross_y = -1;
static bool     prev_pen = false;
static bool     has_data = false;   // true once at least one valid touch read

// ---------------------------------------------------------------------------
// Internal draw helpers
// ---------------------------------------------------------------------------

static void draw_swatch(int16_t x, int16_t y, uint16_t color)
{
    // 8×8 colored square used as row identifier (replaces text label)
    GUI_FillRectColor((uint16_t)x, (uint16_t)y,
                      (uint16_t)(x + 8), (uint16_t)(y + 8), color);
}

static void draw_bar(int16_t bar_y, uint16_t raw, uint16_t bar_color)
{
    uint16_t w = (raw > 0)
                 ? (uint16_t)((uint32_t)raw * (LCD_WIDTH - 4u) / 4095u)
                 : 0u;
    GUI_FillRectColor(2u, (uint16_t)bar_y,
                      (uint16_t)(2u + w), (uint16_t)(bar_y + 10), bar_color);
    GUI_FillRectColor((uint16_t)(2u + w), (uint16_t)bar_y,
                      (uint16_t)(LCD_WIDTH - 2u), (uint16_t)(bar_y + 10), COL_BAR_BG);
}

static void draw_crosshair(int16_t x, int16_t y, uint16_t color)
{
    if (y < CROSS_Y0 || y > FOOTER_Y - 2) return;
    int16_t r = 10;
    int16_t x0 = (x - r < 0)           ? 0          : (x - r);
    int16_t x1 = (x + r >= LCD_WIDTH)  ? LCD_WIDTH-1 : (x + r);
    int16_t y0 = (y - r < CROSS_Y0)    ? CROSS_Y0   : (y - r);
    int16_t y1 = (y + r > FOOTER_Y-2)  ? FOOTER_Y-2  : (y + r);
    GUI_SetColor(color);
    GUI_DrawLine((uint16_t)x0, (uint16_t)y,  (uint16_t)x1, (uint16_t)y);
    GUI_DrawLine((uint16_t)x,  (uint16_t)y0, (uint16_t)x,  (uint16_t)y1);
}

static void redraw_values(void)
{
    // Row RAW — white
    draw_swatch(COL_LABEL, ROW_RAW_Y + 5, COL_RAW);
    seg7_draw_u16(COL_X_VAL, ROW_RAW_Y, raw_x, COL_RAW);
    seg7_draw_u16(COL_Y_VAL, ROW_RAW_Y, raw_y, COL_RAW);

    // Row MIN — green
    uint16_t show_min_x = (min_x == 0xFFFFu) ? 0u : min_x;
    uint16_t show_min_y = (min_y == 0xFFFFu) ? 0u : min_y;
    draw_swatch(COL_LABEL, ROW_MIN_Y + 5, COL_MIN);
    seg7_draw_u16(COL_X_VAL, ROW_MIN_Y, show_min_x, COL_MIN);
    seg7_draw_u16(COL_Y_VAL, ROW_MIN_Y, show_min_y, COL_MIN);

    // Row MAX — red
    draw_swatch(COL_LABEL, ROW_MAX_Y + 5, COL_MAX);
    seg7_draw_u16(COL_X_VAL, ROW_MAX_Y, max_x, COL_MAX);
    seg7_draw_u16(COL_Y_VAL, ROW_MAX_Y, max_y, COL_MAX);
}

static void draw_static_frame(void)
{
    GUI_Clear((uint16_t)COL_BG);

    // Title bar
    GUI_FillRectColor(0, 0, LCD_WIDTH, TITLE_H, COL_TITLE_BG);

    // "CALIB" spelled in tiny blocks in the title bar (no font needed)
    // Just draw a row of 5 colored blocks as a visual placeholder
    for (int i = 0; i < 5; i++)
        GUI_FillRectColor((uint16_t)(10 + i * 14), 6u, (uint16_t)(10 + i * 14 + 10), 18u,
                          (uint16_t)(0x07FF - i * 0x0400));  // cyan gradient

    // Separator lines
    GUI_FillRectColor(0, (uint16_t)TITLE_H, LCD_WIDTH, (uint16_t)(TITLE_H + 1), 0x4228u);
    GUI_FillRectColor(0, (uint16_t)(BAR_X_Y - 2), LCD_WIDTH, (uint16_t)(BAR_X_Y - 1), 0x4228u);
    GUI_FillRectColor(0, (uint16_t)(CROSS_Y0 - 2), LCD_WIDTH, (uint16_t)(CROSS_Y0 - 1), 0x4228u);
    GUI_FillRectColor(0, (uint16_t)FOOTER_Y, LCD_WIDTH, LCD_HEIGHT, 0x2104u);

    // Column header swatches (X = cyan, Y = magenta)
    GUI_FillRectColor((uint16_t)COL_X_VAL, (uint16_t)(TITLE_H + 2),
                      (uint16_t)(COL_X_VAL + NUM4_W), (uint16_t)(TITLE_H + 5), COL_BAR_X);
    GUI_FillRectColor((uint16_t)COL_Y_VAL, (uint16_t)(TITLE_H + 2),
                      (uint16_t)(COL_Y_VAL + NUM4_W), (uint16_t)(TITLE_H + 5), COL_BAR_Y);

    // Bar labels (small colored squares left of each bar)
    GUI_FillRectColor(0, (uint16_t)BAR_X_Y, 2u, (uint16_t)(BAR_X_Y + 10), COL_BAR_X);
    GUI_FillRectColor(0, (uint16_t)BAR_Y_Y, 2u, (uint16_t)(BAR_Y_Y + 10), COL_BAR_Y);

    // Crosshair area hint: 4 small corner marks
    int16_t m = 8;
    GUI_SetColor(0x4228u);
    GUI_DrawLine((uint16_t)0,              (uint16_t)CROSS_Y0,      (uint16_t)m,             (uint16_t)CROSS_Y0);
    GUI_DrawLine((uint16_t)0,              (uint16_t)CROSS_Y0,      (uint16_t)0,              (uint16_t)(CROSS_Y0 + m));
    GUI_DrawLine((uint16_t)(LCD_WIDTH - m),(uint16_t)CROSS_Y0,      (uint16_t)(LCD_WIDTH - 1),(uint16_t)CROSS_Y0);
    GUI_DrawLine((uint16_t)(LCD_WIDTH - 1),(uint16_t)CROSS_Y0,      (uint16_t)(LCD_WIDTH - 1),(uint16_t)(CROSS_Y0 + m));
    GUI_DrawLine((uint16_t)0,              (uint16_t)(FOOTER_Y - m),(uint16_t)0,              (uint16_t)(FOOTER_Y - 1));
    GUI_DrawLine((uint16_t)0,              (uint16_t)(FOOTER_Y - 1),(uint16_t)m,              (uint16_t)(FOOTER_Y - 1));
    GUI_DrawLine((uint16_t)(LCD_WIDTH - 1),(uint16_t)(FOOTER_Y - m),(uint16_t)(LCD_WIDTH - 1),(uint16_t)(FOOTER_Y - 1));
    GUI_DrawLine((uint16_t)(LCD_WIDTH - m),(uint16_t)(FOOTER_Y - 1),(uint16_t)(LCD_WIDTH - 1),(uint16_t)(FOOTER_Y - 1));

    // Footer: 3 small colored indicators (ESC hint)
    GUI_FillRectColor(4u, (uint16_t)(FOOTER_Y + 3), 12u, (uint16_t)(LCD_HEIGHT - 3), 0xF800u);  // red ESC
    GUI_FillRectColor(16u,(uint16_t)(FOOTER_Y + 3), 24u, (uint16_t)(LCD_HEIGHT - 3), 0x4228u);
    GUI_FillRectColor(28u,(uint16_t)(FOOTER_Y + 3), 60u, (uint16_t)(LCD_HEIGHT - 3), 0x2104u);
}

// ---------------------------------------------------------------------------
// Scene API
// ---------------------------------------------------------------------------

void SceneCalib_OnEnter(void)
{
    raw_x = raw_y = 0;
    min_x = 0xFFFFu; max_x = 0u;
    min_y = 0xFFFFu; max_y = 0u;
    cross_x = cross_y = -1;
    prev_pen = false;
    has_data = false;

    draw_static_frame();
    redraw_values();
}

void SceneCalib_OnUpdate(uint32_t now_ms)
{
    (void)now_ms;

    bool pen = (XPT2046_Read_Pen() == 0);   // TPEN active-low

    if (pen) {
        // 0xD0 = physical vertical axis → screen Y
        // 0x90 = physical horizontal axis → screen X
        uint16_t ch_vert = XPT2046_Repeated_Compare_AD(CAL_X_CMD);   // 0xD0
        uint16_t ch_horiz = XPT2046_Repeated_Compare_AD(CAL_Y_CMD);  // 0x90

        if (ch_vert != 0 && ch_horiz != 0) {
            raw_x = ch_horiz;   // horizontal → displayed as X
            raw_y = ch_vert;    // vertical   → displayed as Y
            has_data = true;

            if (ch_horiz < min_x) min_x = ch_horiz;
            if (ch_horiz > max_x) max_x = ch_horiz;
            if (ch_vert  < min_y) min_y = ch_vert;
            if (ch_vert  > max_y) max_y = ch_vert;

            draw_bar(BAR_X_Y, ch_horiz, COL_BAR_X);
            draw_bar(BAR_Y_Y, ch_vert,  COL_BAR_Y);
            redraw_values();

            // Map into crosshair area: horizontal→screen X, vertical→screen Y
            int32_t cross_h = FOOTER_Y - CROSS_Y0;
            int32_t nx_i = (int32_t)(ch_horiz - 200) * LCD_WIDTH / (3900 - 200);
            int32_t ny_i = CROSS_Y0 + (int32_t)(3900 - ch_vert) * cross_h / (3900 - 200);
            int16_t nx = (nx_i < 0) ? 0 : (nx_i >= LCD_WIDTH  ? LCD_WIDTH  - 1 : (int16_t)nx_i);
            int16_t ny = (ny_i < CROSS_Y0) ? (int16_t)CROSS_Y0 : (ny_i > FOOTER_Y - 2 ? (int16_t)(FOOTER_Y - 2) : (int16_t)ny_i);

            if (cross_x >= 0)
                draw_crosshair(cross_x, cross_y, (uint16_t)COL_BG);
            draw_crosshair(nx, ny, COL_CROSS);
            cross_x = nx;
            cross_y = ny;
        }
    }

    if (!pen) prev_pen = false;
    else      prev_pen = true;
}

void SceneCalib_OnExit(void)
{
    if (!has_data || min_x >= max_x || min_y >= max_y)
        return;   // nothing valid to save

    // Brief green flash to signal save
    GUI_FillRectColor(0, 0, LCD_WIDTH, LCD_HEIGHT, (uint16_t)COL_SAVED);

    Settings_t s;
    s.touch_x_min = min_x;
    s.touch_x_max = max_x;
    s.touch_y_min = min_y;
    s.touch_y_max = max_y;
    Settings_Save(&s);

    // Apply immediately — no reboot needed
    Nav_SetCalibration(min_x, max_x, min_y, max_y);
}
