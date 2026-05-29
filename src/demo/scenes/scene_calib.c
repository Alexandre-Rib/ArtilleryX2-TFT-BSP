/**
 * @file    scene_calib.c
 * @brief   Scene: guided 4-corner touch calibration
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Guided procedure — no text required:
 *    1. A large corner target blinks (yellow = touch me)
 *    2. User touches that corner
 *    3. Corner turns green (confirmed), next corner blinks
 *    4. After 4 corners: auto-save to W25Q64 + apply via Nav_SetCalibration
 *    5. All corners green + 7-seg values shown in center -> ESC to return
 *
 *  Sequence: TL -> TR -> BR -> BL (clockwise)
 */

#include "scene_calib.h"
#include "settings.h"
#include "ui_nav.h"
#include "xpt2046.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "mks_tft28.h"
#include "delay.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ---------------------------------------------------------------------------
// ADC channels — 0xD0 = vertical axis, 0x90 = horizontal axis
// ---------------------------------------------------------------------------
#define CAL_VERT_CMD   0xD0
#define CAL_HORIZ_CMD  0x90

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
#define CORNER_SZ  64    // size of each corner touch target (px)

// Corner top-left positions: TL, TR, BR, BL (clockwise)
static const int16_t CX[4] = { 0,
                                LCD_WIDTH  - CORNER_SZ,
                                LCD_WIDTH  - CORNER_SZ,
                                0 };
static const int16_t CY[4] = { 0,
                                0,
                                LCD_HEIGHT - CORNER_SZ,
                                LCD_HEIGHT - CORNER_SZ };

// Progress dot row — 4 x 18 px squares, centered horizontally + vertically
#define DOT_SZ   18
#define DOT_GAP  10
#define DOT_ROW_W  (4 * DOT_SZ + 3 * DOT_GAP)
#define DOT_X0   ((LCD_WIDTH  - DOT_ROW_W) / 2)
#define DOT_Y0   ((LCD_HEIGHT - DOT_SZ)    / 2)

// 7-segment for summary (small version)
#define SEG_W   10
#define SEG_H   16
#define SEG_T    2
#define SEG_M   (SEG_H / 2)
#define SEG_GAP  3
#define NUM4_W  (4 * SEG_W + 3 * SEG_GAP)

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------
#define COL_BG       0x0000u   // black
#define COL_ACTIVE   0xFFE0u   // yellow — blinks on current target
#define COL_DIM      0x2945u   // dark blue-gray — active blink-off
#define COL_DONE     0x07E0u   // green — captured corner
#define COL_INACTIVE 0x18C3u   // very dark gray — future corners
#define COL_CROSS    0xFFFFu   // white crosshair on corner
#define COL_MIN      0x07E0u   // green 7-seg
#define COL_MAX      0xF800u   // red 7-seg
#define COL_LABEL    0x528Au   // gray label swatches

// ---------------------------------------------------------------------------
// 7-segment renderer (compact copy for this file)
// ---------------------------------------------------------------------------
static const uint8_t SEG7[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

static void seg_digit(int16_t x, int16_t y, uint8_t d, uint16_t col)
{
    if (d > 9) return;
    uint8_t m = SEG7[d];
    uint16_t bg = COL_BG;
    GUI_FillRectColor(x+SEG_T,     y,          x+SEG_W-SEG_T, y+SEG_T,          (m&0x01)?col:bg);
    GUI_FillRectColor(x+SEG_W-SEG_T, y,        x+SEG_W,       y+SEG_M,          (m&0x02)?col:bg);
    GUI_FillRectColor(x+SEG_W-SEG_T, y+SEG_M,  x+SEG_W,       y+SEG_H,          (m&0x04)?col:bg);
    GUI_FillRectColor(x+SEG_T,     y+SEG_H-SEG_T, x+SEG_W-SEG_T, y+SEG_H,      (m&0x08)?col:bg);
    GUI_FillRectColor(x,           y+SEG_M,    x+SEG_T,       y+SEG_H,          (m&0x10)?col:bg);
    GUI_FillRectColor(x,           y,          x+SEG_T,       y+SEG_M,          (m&0x20)?col:bg);
    GUI_FillRectColor(x+SEG_T,     y+SEG_M-1,  x+SEG_W-SEG_T, y+SEG_M+SEG_T-1,(m&0x40)?col:bg);
}

static void seg_u16(int16_t x, int16_t y, uint16_t v, uint16_t col)
{
    int16_t s = SEG_W + SEG_GAP;
    seg_digit(x,       y, (v/1000)%10, col);
    seg_digit(x+s,     y, (v/100)%10,  col);
    seg_digit(x+2*s,   y, (v/10)%10,   col);
    seg_digit(x+3*s,   y,  v%10,        col);
}

// ---------------------------------------------------------------------------
// Corner drawing helpers
// ---------------------------------------------------------------------------
static void draw_corner(int step, uint16_t fill_col)
{
    int16_t x = CX[step], y = CY[step];

    // Large fill
    GUI_FillRectColor((uint16_t)x, (uint16_t)y,
                      (uint16_t)(x + CORNER_SZ), (uint16_t)(y + CORNER_SZ),
                      fill_col);

    // White crosshair at center of corner
    int16_t cx = x + CORNER_SZ / 2;
    int16_t cy = y + CORNER_SZ / 2;
    GUI_SetColor(COL_CROSS);
    GUI_DrawLine((uint16_t)(cx - 12), (uint16_t)cy, (uint16_t)(cx + 12), (uint16_t)cy);
    GUI_DrawLine((uint16_t)cx, (uint16_t)(cy - 12), (uint16_t)cx, (uint16_t)(cy + 12));
    GUI_FillRectColor((uint16_t)(cx - 3), (uint16_t)(cy - 3),
                      (uint16_t)(cx + 3), (uint16_t)(cy + 3), COL_CROSS);
}

static void draw_all_corners(uint8_t current_step, bool blink_on,
                              const bool done[4])
{
    for (int i = 0; i < 4; i++) {
        uint16_t col;
        if (done[i]) {
            col = COL_DONE;
        } else if (i == (int)current_step) {
            col = blink_on ? COL_ACTIVE : COL_DIM;
        } else {
            col = COL_INACTIVE;
        }
        draw_corner(i, col);
    }
}

static void draw_progress_dots(const bool done[4], uint8_t current_step)
{
    // Clear dot row background first
    GUI_FillRectColor((uint16_t)(DOT_X0 - 4), (uint16_t)(DOT_Y0 - 4),
                      (uint16_t)(DOT_X0 + DOT_ROW_W + 4), (uint16_t)(DOT_Y0 + DOT_SZ + 4),
                      COL_BG);

    for (int i = 0; i < 4; i++) {
        int16_t dx = DOT_X0 + i * (DOT_SZ + DOT_GAP);
        uint16_t col = done[i] ? COL_DONE :
                       (i == (int)current_step) ? COL_ACTIVE : COL_INACTIVE;
        GUI_FillRectColor((uint16_t)dx, (uint16_t)DOT_Y0,
                          (uint16_t)(dx + DOT_SZ), (uint16_t)(DOT_Y0 + DOT_SZ),
                          col);
    }
}

static void draw_summary(uint16_t xmin, uint16_t xmax,
                          uint16_t ymin, uint16_t ymax)
{
    // Center area between corners: X: CORNER_SZ..LCD_W-CORNER_SZ, Y: CORNER_SZ..LCD_H-CORNER_SZ
    // 4 rows of 7-seg values (x_min, x_max, y_min, y_max)
    int16_t area_w = LCD_WIDTH  - 2 * CORNER_SZ;
    int16_t area_h = LCD_HEIGHT - 2 * CORNER_SZ;
    int16_t rows_h = 4 * SEG_H + 3 * 6;  // 4 rows + gaps
    int16_t start_x = CORNER_SZ + (area_w - NUM4_W - 14) / 2;
    int16_t start_y = CORNER_SZ + (area_h - rows_h) / 2;

    // Clear center area
    GUI_FillRectColor((uint16_t)CORNER_SZ, (uint16_t)CORNER_SZ,
                      (uint16_t)(LCD_WIDTH  - CORNER_SZ),
                      (uint16_t)(LCD_HEIGHT - CORNER_SZ), COL_BG);

    // Label swatches (small 8x8 squares) + values
    int16_t row = start_y;
    int16_t step = SEG_H + 6;

    GUI_FillRectColor((uint16_t)start_x, (uint16_t)(row+4),
                      (uint16_t)(start_x+8), (uint16_t)(row+12), COL_MIN);
    seg_u16(start_x + 12, row, xmin, COL_MIN);  row += step;

    GUI_FillRectColor((uint16_t)start_x, (uint16_t)(row+4),
                      (uint16_t)(start_x+8), (uint16_t)(row+12), COL_MAX);
    seg_u16(start_x + 12, row, xmax, COL_MAX);  row += step;

    GUI_FillRectColor((uint16_t)start_x, (uint16_t)(row+4),
                      (uint16_t)(start_x+8), (uint16_t)(row+12), COL_MIN);
    seg_u16(start_x + 12, row, ymin, COL_MIN);  row += step;

    GUI_FillRectColor((uint16_t)start_x, (uint16_t)(row+4),
                      (uint16_t)(start_x+8), (uint16_t)(row+12), COL_MAX);
    seg_u16(start_x + 12, row, ymax, COL_MAX);
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static uint8_t  cal_step;           // 0-3: current corner, 4: done
static bool     corner_done[4];
static uint16_t glob_min_x, glob_max_x, glob_min_y, glob_max_y;
static bool     blink_on;
static uint32_t last_blink_ms;
static bool     prev_pen;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void SceneCalib_OnEnter(void)
{
    cal_step   = 0;
    blink_on   = true;
    prev_pen   = false;
    last_blink_ms = 0;
    glob_min_x = 0xFFFFu; glob_max_x = 0u;
    glob_min_y = 0xFFFFu; glob_max_y = 0u;
    memset(corner_done, 0, sizeof(corner_done));

    GUI_Clear(COL_BG);
    draw_all_corners(0, true, corner_done);
    draw_progress_dots(corner_done, 0);
}

void SceneCalib_OnUpdate(uint32_t now_ms)
{
    if (cal_step >= 4) return;   // all done, wait for ESC

    // Blink current corner
    bool blink = ((now_ms / 400u) % 2u) == 0u;
    if (blink != blink_on) {
        blink_on = blink;
        uint16_t col = blink_on ? COL_ACTIVE : COL_DIM;
        draw_corner(cal_step, col);
    }

    // Touch detection (rising edge only)
    bool pen = (XPT2046_Read_Pen() == 0);

    if (pen && !prev_pen) {
        uint16_t ch_v = XPT2046_Repeated_Compare_AD(CAL_VERT_CMD);    // 0xD0
        uint16_t ch_h = XPT2046_Repeated_Compare_AD(CAL_HORIZ_CMD);   // 0x90

        if (ch_v != 0 && ch_h != 0) {
            // Update global min/max
            if (ch_h < glob_min_x) glob_min_x = ch_h;
            if (ch_h > glob_max_x) glob_max_x = ch_h;
            if (ch_v < glob_min_y) glob_min_y = ch_v;
            if (ch_v > glob_max_y) glob_max_y = ch_v;

            // Confirm this corner
            corner_done[cal_step] = true;
            draw_corner(cal_step, COL_DONE);

            cal_step++;
            draw_progress_dots(corner_done, cal_step);

            if (cal_step >= 4) {
                // Brief confirmation flash
                Delay_ms(150);
                GUI_FillRectColor(0, 0, LCD_WIDTH, LCD_HEIGHT, COL_DONE);
                Delay_ms(150);

                // Save calibration
                if (glob_min_x < glob_max_x && glob_min_y < glob_max_y) {
                    Settings_t s;
                    s.touch_x_min = glob_min_x;
                    s.touch_x_max = glob_max_x;
                    s.touch_y_min = glob_min_y;
                    s.touch_y_max = glob_max_y;
                    Settings_Save(&s);
                    Nav_SetCalibration(glob_min_x, glob_max_x,
                                       glob_min_y, glob_max_y);
                }

                // Redraw all corners green + summary
                GUI_Clear(COL_BG);
                for (int i = 0; i < 4; i++)
                    draw_corner(i, COL_DONE);
                draw_progress_dots(corner_done, 4);
                draw_summary(glob_min_x, glob_max_x, glob_min_y, glob_max_y);
            }
        }
    }

    prev_pen = pen;
}

void SceneCalib_OnExit(void)
{
    // Calibration already saved in OnUpdate when cal_step reached 4
}
