/**
 * @file    scene_calib.c
 * @brief   Scene: XPT2046 touch screen calibration helper
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#include "scene_calib.h"
#include "xpt2046.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "font_render.h"
#include "mks_tft28.h"
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Touch ADC channel commands (XPT2046)
// ---------------------------------------------------------------------------
#define CAL_X_CMD  0xD0
#define CAL_Y_CMD  0x90

// Current calibration constants (must match ui_nav.c — used for crosshair)
#define CAL_X_MIN  200u
#define CAL_X_MAX  3900u
#define CAL_Y_MIN  200u
#define CAL_Y_MAX  3900u

// Layout
#define TITLE_H    28
#define HINT_Y     (TITLE_H + 4)
#define DATA_Y     (HINT_Y + 20)
#define BAR_X_Y    (DATA_Y + 70)
#define BAR_Y_Y    (BAR_X_Y + 22)
#define CROSS_AREA_Y (BAR_Y_Y + 28)
#define FOOTER_Y   (LCD_HEIGHT - 16)

// Colors
#define COL_TITLE_BG  0x2965u
#define COL_BG        0x0841u
#define COL_RAW       0xFFFFu   // white
#define COL_MIN       0x07E0u   // green
#define COL_MAX       0xF800u   // red
#define COL_HINT      0xFFE0u   // yellow
#define COL_FOOTER    0x528Au   // gray
#define COL_BAR_BG    0x2104u
#define COL_BAR_X     0x07FFu   // cyan bar for X
#define COL_BAR_Y     0xF81Fu   // magenta bar for Y
#define COL_CROSS     0xFFE0u   // yellow crosshair
#define COL_CROSS_OLD 0x2104u   // erase color (matches bg)

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static uint16_t raw_x   = 0;
static uint16_t raw_y   = 0;
static uint16_t min_x   = 0xFFFFu;
static uint16_t max_x   = 0u;
static uint16_t min_y   = 0xFFFFu;
static uint16_t max_y   = 0u;

static int16_t  cross_x = -1;   // last crosshair position (-1 = none)
static int16_t  cross_y = -1;

static bool     prev_pen = false;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static int16_t map_to_screen_x(uint16_t raw)
{
    if (raw <= CAL_X_MIN) return 0;
    if (raw >= CAL_X_MAX) return LCD_WIDTH - 1;
    return (int16_t)((uint32_t)(raw - CAL_X_MIN) * LCD_WIDTH
                     / (CAL_X_MAX - CAL_X_MIN));
}

static int16_t map_to_screen_y(uint16_t raw)
{
    if (raw <= CAL_Y_MIN) return 0;
    if (raw >= CAL_Y_MAX) return LCD_HEIGHT - 1;
    return (int16_t)((uint32_t)(raw - CAL_Y_MIN) * LCD_HEIGHT
                     / (CAL_Y_MAX - CAL_Y_MIN));
}

static void draw_crosshair(int16_t x, int16_t y, uint16_t color)
{
    // Only draw inside the crosshair area
    if (y < CROSS_AREA_Y || y > FOOTER_Y - 4) return;
    int16_t r = 8;
    GUI_SetColor(color);
    GUI_DrawLine((uint16_t)(x - r), (uint16_t)y, (uint16_t)(x + r), (uint16_t)y);
    GUI_DrawLine((uint16_t)x, (uint16_t)(y - r), (uint16_t)x, (uint16_t)(y + r));
}

static void draw_bar(int16_t bar_y, uint16_t raw, uint16_t bar_color)
{
    uint16_t bar_w = (raw > 0) ? (uint16_t)((uint32_t)raw * LCD_WIDTH / 4096u) : 0u;
    GUI_FillRectColor(0,     (uint16_t)bar_y, (uint16_t)bar_w,  (uint16_t)(bar_y + 16), bar_color);
    GUI_FillRectColor((uint16_t)bar_w, (uint16_t)bar_y, LCD_WIDTH, (uint16_t)(bar_y + 16), COL_BAR_BG);
}

static void redraw_data(void)
{
    // Clear data zone
    GUI_FillRectColor(0, (uint16_t)DATA_Y, LCD_WIDTH, (uint16_t)(BAR_X_Y - 2), COL_BG);

    setFontSize(FONT_SIZE_NORMAL);
    GUI_SetTextMode(GUI_TEXTMODE_TRANS);

    // Row: RAW  X: ####   Y: ####
    GUI_SetColor(COL_RAW);
    _GUI_DispStringInRect(2,   DATA_Y,      50,  DATA_Y + 16, (const uint8_t *)"RAW");
    _GUI_DispStringInRect(54,  DATA_Y,      90,  DATA_Y + 16, (const uint8_t *)"X:");
    GUI_DispDec(94, DATA_Y, (int32_t)raw_x, 4, 0);
    _GUI_DispStringInRect(170, DATA_Y,      206, DATA_Y + 16, (const uint8_t *)"Y:");
    GUI_DispDec(210, DATA_Y, (int32_t)raw_y, 4, 0);

    // Row: MIN  X: ####   Y: ####
    GUI_SetColor(COL_MIN);
    _GUI_DispStringInRect(2,   DATA_Y + 20, 50,  DATA_Y + 36, (const uint8_t *)"MIN");
    _GUI_DispStringInRect(54,  DATA_Y + 20, 90,  DATA_Y + 36, (const uint8_t *)"X:");
    GUI_DispDec(94, DATA_Y + 20, (int32_t)(min_x == 0xFFFFu ? 0u : min_x), 4, 0);
    _GUI_DispStringInRect(170, DATA_Y + 20, 206, DATA_Y + 36, (const uint8_t *)"Y:");
    GUI_DispDec(210, DATA_Y + 20, (int32_t)(min_y == 0xFFFFu ? 0u : min_y), 4, 0);

    // Row: MAX  X: ####   Y: ####
    GUI_SetColor(COL_MAX);
    _GUI_DispStringInRect(2,   DATA_Y + 40, 50,  DATA_Y + 56, (const uint8_t *)"MAX");
    _GUI_DispStringInRect(54,  DATA_Y + 40, 90,  DATA_Y + 56, (const uint8_t *)"X:");
    GUI_DispDec(94, DATA_Y + 40, (int32_t)max_x, 4, 0);
    _GUI_DispStringInRect(170, DATA_Y + 40, 206, DATA_Y + 56, (const uint8_t *)"Y:");
    GUI_DispDec(210, DATA_Y + 40, (int32_t)max_y, 4, 0);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void SceneCalib_OnEnter(void)
{
    raw_x = raw_y = 0;
    min_x = 0xFFFFu; max_x = 0u;
    min_y = 0xFFFFu; max_y = 0u;
    cross_x = cross_y = -1;
    prev_pen = false;

    GUI_Clear((uint16_t)COL_BG);

    // Title bar
    GUI_FillRectColor(0, 0, LCD_WIDTH, TITLE_H, COL_TITLE_BG);
    setFontSize(FONT_SIZE_NORMAL);
    GUI_SetTextMode(GUI_TEXTMODE_TRANS);
    GUI_SetColor(WHITE);
    _GUI_DispStringInRect(0, 6, LCD_WIDTH, TITLE_H - 4,
                          (const uint8_t *)"TOUCH CALIBRATION");

    // Hint line
    GUI_SetColor((uint16_t)COL_HINT);
    _GUI_DispStringInRect(2, HINT_Y, LCD_WIDTH - 2, HINT_Y + 16,
                          (const uint8_t *)"Tap 4 corners. Note MIN/MAX. ESC = done.");

    // Bar labels
    GUI_SetColor((uint16_t)COL_BAR_X);
    _GUI_DispStringInRect(2, BAR_X_Y, 16, BAR_X_Y + 16, (const uint8_t *)"X");
    GUI_SetColor((uint16_t)COL_BAR_Y);
    _GUI_DispStringInRect(2, BAR_Y_Y, 16, BAR_Y_Y + 16, (const uint8_t *)"Y");

    // Bar backgrounds
    GUI_FillRectColor(18, (uint16_t)BAR_X_Y, LCD_WIDTH, (uint16_t)(BAR_X_Y + 16), COL_BAR_BG);
    GUI_FillRectColor(18, (uint16_t)BAR_Y_Y, LCD_WIDTH, (uint16_t)(BAR_Y_Y + 16), COL_BAR_BG);

    // Footer
    GUI_SetColor((uint16_t)COL_FOOTER);
    _GUI_DispStringInRect(2, FOOTER_Y, LCD_WIDTH - 2, LCD_HEIGHT - 2,
                          (const uint8_t *)"ui_nav.c: TOUCH_X/Y_MIN/MAX");

    redraw_data();
}

void SceneCalib_OnUpdate(uint32_t now_ms)
{
    (void)now_ms;

    bool pen = (XPT2046_Read_Pen() == 0);   // TPEN active-low

    if (pen) {
        uint16_t rx = XPT2046_Repeated_Compare_AD(CAL_X_CMD);
        uint16_t ry = XPT2046_Repeated_Compare_AD(CAL_Y_CMD);

        if (rx != 0 && ry != 0) {
            raw_x = rx;
            raw_y = ry;

            // Update min/max
            if (rx < min_x) min_x = rx;
            if (rx > max_x) max_x = rx;
            if (ry < min_y) min_y = ry;
            if (ry > max_y) max_y = ry;

            // Update ADC bars (offset 18px to clear the "X"/"Y" label)
            draw_bar(BAR_X_Y, rx, COL_BAR_X);
            draw_bar(BAR_Y_Y, ry, COL_BAR_Y);

            // Move crosshair: erase previous, draw new
            int16_t nx = map_to_screen_x(rx);
            int16_t ny = map_to_screen_y(ry);

            // Constrain crosshair Y to the dedicated area
            if (ny < CROSS_AREA_Y) ny = (int16_t)CROSS_AREA_Y + 8;
            if (ny > FOOTER_Y - 4) ny = (int16_t)(FOOTER_Y - 4);

            if (cross_x >= 0)
                draw_crosshair(cross_x, cross_y, (uint16_t)COL_BG);  // erase old

            draw_crosshair(nx, ny, COL_CROSS);
            cross_x = nx;
            cross_y = ny;

            redraw_data();
        }
    }

    if (!pen && prev_pen) {
        // Pen lifted — freeze display, keep crosshair
    }

    prev_pen = pen;
}

void SceneCalib_OnExit(void)
{
    // nothing to release
}
