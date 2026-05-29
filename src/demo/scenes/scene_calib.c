/**
 * @file    scene_calib.c
 * @brief   Scene: touch calibration — live display + guided 4-corner procedure
 * @version 3.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Two sub-modes, both accessible from this scene:
 *
 *  LIVE mode (entry point):
 *    - Live RAW/MIN/MAX 7-seg values, ADC bars, crosshair
 *    - Footer: [EXIT→menu (red)] [PROCEDURE (cyan)]
 *    - ESC key = back to main menu
 *
 *  PROCEDURE mode (launched from LIVE footer button or Enter key):
 *    - Guided 4-corner calibration, clockwise TL→TR→BR→BL
 *    - Each corner blinks yellow until touched, then turns green
 *    - Footer: [QUIT→LIVE (red)] [SAVE to flash (green)]
 *    - SAVE: confirms with green flash, stays in procedure
 *    - QUIT/ESC: returns to LIVE mode
 *
 *  Footer button detection uses calibrated raw ADC thresholds
 *  (Nav_GetCalibration) so accuracy improves after calibration.
 */

#include "scene_calib.h"
#include "settings.h"
#include "ui_nav.h"
#include "xpt2046.h"
#include "keyboard.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "font_render.h"
#include "mks_tft28.h"
#include "delay.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ---------------------------------------------------------------------------
// ADC channels
// ---------------------------------------------------------------------------
#define CAL_VERT_CMD   0xD0   // physical vertical axis  -> screen Y (FLIP_Y)
#define CAL_HORIZ_CMD  0x90   // physical horizontal axis -> screen X

// ---------------------------------------------------------------------------
// Sub-modes
// ---------------------------------------------------------------------------
typedef enum { MODE_LIVE, MODE_PROC } CalibMode_t;

// ---------------------------------------------------------------------------
// Layout — shared
// ---------------------------------------------------------------------------
#define FOOTER_H    24
#define FOOTER_Y    (LCD_HEIGHT - FOOTER_H)

// ---------------------------------------------------------------------------
// 7-segment renderer (all display is primitives — no font atlas needed)
// ---------------------------------------------------------------------------
#define SEG_W   12
#define SEG_H   18
#define SEG_T    2
#define SEG_M   (SEG_H / 2)
#define SEG_GAP  3
#define NUM4_W  (4 * SEG_W + 3 * SEG_GAP)

static const uint8_t SEG7[10] = {
    0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F
};

static void seg_digit(int16_t x, int16_t y, uint8_t d, uint16_t col)
{
    if (d > 9) return;
    uint8_t  m  = SEG7[d];
    uint16_t bg = BLACK;
    GUI_FillRectColor(x+SEG_T,       y,             x+SEG_W-SEG_T, y+SEG_T,            (m&0x01)?col:bg);
    GUI_FillRectColor(x+SEG_W-SEG_T, y,             x+SEG_W,       y+SEG_M,            (m&0x02)?col:bg);
    GUI_FillRectColor(x+SEG_W-SEG_T, y+SEG_M,       x+SEG_W,       y+SEG_H,            (m&0x04)?col:bg);
    GUI_FillRectColor(x+SEG_T,       y+SEG_H-SEG_T, x+SEG_W-SEG_T,y+SEG_H,            (m&0x08)?col:bg);
    GUI_FillRectColor(x,             y+SEG_M,        x+SEG_T,       y+SEG_H,            (m&0x10)?col:bg);
    GUI_FillRectColor(x,             y,              x+SEG_T,       y+SEG_M,            (m&0x20)?col:bg);
    GUI_FillRectColor(x+SEG_T,       y+SEG_M-1,     x+SEG_W-SEG_T,y+SEG_M+SEG_T-1,   (m&0x40)?col:bg);
}

static void seg_u16(int16_t x, int16_t y, uint16_t v, uint16_t col)
{
    int16_t s = SEG_W + SEG_GAP;
    seg_digit(x,     y, (v/1000)%10, col);
    seg_digit(x+s,   y, (v/100)%10,  col);
    seg_digit(x+2*s, y, (v/10)%10,   col);
    seg_digit(x+3*s, y,  v%10,        col);
}

// ---------------------------------------------------------------------------
// Footer button drawing
// ---------------------------------------------------------------------------
// Colors
#define COL_BTN_EXIT  0x9000u  // dark red
#define COL_BTN_PROC  0x0318u  // dark cyan
#define COL_BTN_QUIT  0x9000u  // dark red
#define COL_BTN_SAVE  0x03E0u  // dark green
#define COL_ICON      0xFFFFu  // white icon

static void draw_footer_live(void)
{
    int16_t fy = FOOTER_Y;
    int16_t mid = LCD_WIDTH / 2;

    // Left: EXIT (red) with X icon
    GUI_FillRectColor(0,   fy, mid,        LCD_HEIGHT, COL_BTN_EXIT);
    int16_t cx = mid / 2, cy = fy + FOOTER_H / 2;
    GUI_SetColor(COL_ICON);
    GUI_DrawLine(cx-7, cy-5, cx+7, cy+5);
    GUI_DrawLine(cx+7, cy-5, cx-7, cy+5);

    // Right: PROCEDURE (cyan) with right-arrow icon
    GUI_FillRectColor(mid, fy, LCD_WIDTH, LCD_HEIGHT, COL_BTN_PROC);
    cx = mid + mid / 2;
    GUI_SetColor(COL_ICON);
    GUI_DrawLine(cx-8, cy, cx+6, cy);
    GUI_DrawLine(cx+2, cy-5, cx+8, cy);
    GUI_DrawLine(cx+2, cy+5, cx+8, cy);
}

static void draw_footer_proc(bool save_flash)
{
    int16_t fy = FOOTER_Y;
    int16_t mid = LCD_WIDTH / 2;
    uint16_t save_col = save_flash ? 0xFFE0u : COL_BTN_SAVE;

    // Left: QUIT (red) with left-arrow
    GUI_FillRectColor(0,   fy, mid,        LCD_HEIGHT, COL_BTN_QUIT);
    int16_t cx1 = mid / 2, cy = fy + FOOTER_H / 2;
    GUI_SetColor(COL_ICON);
    GUI_DrawLine(cx1+8, cy, cx1-6, cy);
    GUI_DrawLine(cx1-2, cy-5, cx1-8, cy);
    GUI_DrawLine(cx1-2, cy+5, cx1-8, cy);

    // Right: SAVE (green) with checkmark
    GUI_FillRectColor(mid, fy, LCD_WIDTH, LCD_HEIGHT, save_col);
    int16_t cx2 = mid + mid / 2;
    GUI_SetColor(COL_ICON);
    GUI_DrawLine(cx2-8, cy,   cx2-2, cy+6);
    GUI_DrawLine(cx2-2, cy+6, cx2+8, cy-4);
}

// ---------------------------------------------------------------------------
// Footer touch detection using current calibration
// Returns: 0=not footer, 1=left button, 2=right button
// ---------------------------------------------------------------------------
static uint8_t check_footer_button(uint16_t ch_h, uint16_t ch_v)
{
    uint16_t cx_min, cx_max, cy_min, cy_max;
    Nav_GetCalibration(&cx_min, &cx_max, &cy_min, &cy_max);

    // FLIP_Y=1: physical bottom = LOW ch_v
    // Compute raw ch_v that corresponds to screen Y = FOOTER_Y
    // screen_y = (cy_max - raw_v) * H / (cy_max - cy_min)
    // => raw_v = cy_max - FOOTER_Y * (cy_max - cy_min) / H
    int32_t footer_thresh = (int32_t)cy_max
                          - (int32_t)FOOTER_Y * (cy_max - cy_min) / LCD_HEIGHT;
    if (footer_thresh < 0) footer_thresh = 0;

    if ((int32_t)ch_v > footer_thresh)
        return 0;  // not in footer zone

    uint16_t mid_h = (uint16_t)(((uint32_t)cx_min + cx_max) / 2u);
    return (ch_h < mid_h) ? 1u : 2u;
}

// ===========================================================================
// LIVE MODE
// ===========================================================================

// Layout rows
#define LIVE_TITLE_H   24
#define LIVE_ROW_RAW   (LIVE_TITLE_H + 4)
#define LIVE_ROW_MIN   (LIVE_ROW_RAW + SEG_H + 6)
#define LIVE_ROW_MAX   (LIVE_ROW_MIN + SEG_H + 6)
#define LIVE_BAR_X_Y   (LIVE_ROW_MAX + SEG_H + 6)
#define LIVE_BAR_Y_Y   (LIVE_BAR_X_Y + 12)
#define LIVE_CROSS_Y0  (LIVE_BAR_Y_Y + 14)

// Column positions (label swatch 10px + gap)
#define LIVE_COL_LBL   2
#define LIVE_COL_X     14
#define LIVE_COL_Y     (LIVE_COL_X + NUM4_W + 10)

// Colors
#define COL_BG        0x0000u
#define COL_RAW       0xFFFFu
#define COL_MIN       0x07E0u
#define COL_MAX       0xF800u
#define COL_BAR_X     0x07FFu
#define COL_BAR_Y     0xF81Fu
#define COL_BAR_BG    0x2104u
#define COL_CROSS     0xFFE0u

static uint16_t live_raw_x, live_raw_y;
static uint16_t live_min_x, live_max_x, live_min_y, live_max_y;
static int16_t  live_cx = -1, live_cy = -1;  // last crosshair pos
static bool     live_prev_pen;

static void live_draw_bar(int16_t by, uint16_t raw, uint16_t col)
{
    uint16_t w = (raw > 0) ? (uint16_t)((uint32_t)raw*(LCD_WIDTH-4)/4095u) : 0u;
    GUI_FillRectColor(2u, (uint16_t)by, (uint16_t)(2+w),       (uint16_t)(by+10), col);
    GUI_FillRectColor((uint16_t)(2+w), (uint16_t)by, LCD_WIDTH-2u, (uint16_t)(by+10), COL_BAR_BG);
}

static void live_draw_crosshair(int16_t x, int16_t y, uint16_t col)
{
    if (y < LIVE_CROSS_Y0 || y > FOOTER_Y - 2) return;
    int16_t r = 10;
    int16_t x0 = (x-r<0)?0:(x-r), x1 = (x+r>=LCD_WIDTH)?LCD_WIDTH-1:(x+r);
    int16_t y0 = (y-r<LIVE_CROSS_Y0)?LIVE_CROSS_Y0:(y-r);
    int16_t y1 = (y+r>FOOTER_Y-2)?FOOTER_Y-2:(y+r);
    GUI_SetColor(col);
    GUI_DrawLine(x0, y, x1, y);
    GUI_DrawLine(x, y0, x, y1);
}

static void live_draw_values(void)
{
    GUI_FillRectColor(0, LIVE_ROW_RAW, LCD_WIDTH, LIVE_BAR_X_Y - 2, COL_BG);

    // Swatches + 7-seg rows
    GUI_FillRectColor(LIVE_COL_LBL, LIVE_ROW_RAW+5, LIVE_COL_LBL+8, LIVE_ROW_RAW+13, COL_RAW);
    seg_u16(LIVE_COL_X, LIVE_ROW_RAW, live_raw_x, COL_RAW);
    seg_u16(LIVE_COL_Y, LIVE_ROW_RAW, live_raw_y, COL_RAW);

    uint16_t mx = (live_min_x==0xFFFFu)?0u:live_min_x;
    uint16_t my = (live_min_y==0xFFFFu)?0u:live_min_y;
    GUI_FillRectColor(LIVE_COL_LBL, LIVE_ROW_MIN+5, LIVE_COL_LBL+8, LIVE_ROW_MIN+13, COL_MIN);
    seg_u16(LIVE_COL_X, LIVE_ROW_MIN, mx, COL_MIN);
    seg_u16(LIVE_COL_Y, LIVE_ROW_MIN, my, COL_MIN);

    GUI_FillRectColor(LIVE_COL_LBL, LIVE_ROW_MAX+5, LIVE_COL_LBL+8, LIVE_ROW_MAX+13, COL_MAX);
    seg_u16(LIVE_COL_X, LIVE_ROW_MAX, live_max_x, COL_MAX);
    seg_u16(LIVE_COL_Y, LIVE_ROW_MAX, live_max_y, COL_MAX);
}

static void live_draw_static(void)
{
    GUI_Clear(COL_BG);

    // Title strip
    GUI_FillRectColor(0, 0, LCD_WIDTH, LIVE_TITLE_H, 0x2965u);

    // Column header stripes under title
    GUI_FillRectColor(LIVE_COL_X,  LIVE_TITLE_H, LIVE_COL_X+NUM4_W,  LIVE_TITLE_H+3, COL_BAR_X);
    GUI_FillRectColor(LIVE_COL_Y,  LIVE_TITLE_H, LIVE_COL_Y+NUM4_W,  LIVE_TITLE_H+3, COL_BAR_Y);

    // Bar labels
    GUI_FillRectColor(0, LIVE_BAR_X_Y, 2u, LIVE_BAR_X_Y+10, COL_BAR_X);
    GUI_FillRectColor(0, LIVE_BAR_Y_Y, 2u, LIVE_BAR_Y_Y+10, COL_BAR_Y);

    // Crosshair area corner marks
    GUI_SetColor(0x4228u);
    GUI_DrawLine(0, LIVE_CROSS_Y0,   8, LIVE_CROSS_Y0);
    GUI_DrawLine(0, LIVE_CROSS_Y0,   0, LIVE_CROSS_Y0+8);
    GUI_DrawLine(LCD_WIDTH-8, LIVE_CROSS_Y0, LCD_WIDTH-1, LIVE_CROSS_Y0);
    GUI_DrawLine(LCD_WIDTH-1, LIVE_CROSS_Y0, LCD_WIDTH-1, LIVE_CROSS_Y0+8);

    // Separator before footer
    GUI_FillRectColor(0, FOOTER_Y-1, LCD_WIDTH, FOOTER_Y, 0x4228u);

    draw_footer_live();
}

static void live_update(uint32_t now_ms)
{
    (void)now_ms;
    bool pen = (XPT2046_Read_Pen() == 0);

    if (pen) {
        uint16_t ch_v = XPT2046_Repeated_Compare_AD(CAL_VERT_CMD);
        uint16_t ch_h = XPT2046_Repeated_Compare_AD(CAL_HORIZ_CMD);

        if (ch_v != 0 && ch_h != 0) {
            // Check footer buttons on rising edge
            if (!live_prev_pen) {
                uint8_t btn = check_footer_button(ch_h, ch_v);
                if (btn != 0) {
                    live_prev_pen = true;
                    // btn 2 (right) = launch procedure — handled by caller
                    // btn 1 (left)  = ESC, handled by demo_app
                    return;  // don't update calibration data from footer touch
                }
            }

            // Calibration data update
            live_raw_x = ch_h;
            live_raw_y = ch_v;
            if (ch_h < live_min_x) live_min_x = ch_h;
            if (ch_h > live_max_x) live_max_x = ch_h;
            if (ch_v < live_min_y) live_min_y = ch_v;
            if (ch_v > live_max_y) live_max_y = ch_v;

            live_draw_bar(LIVE_BAR_X_Y, ch_h, COL_BAR_X);
            live_draw_bar(LIVE_BAR_Y_Y, ch_v, COL_BAR_Y);
            live_draw_values();

            // Crosshair — use current calibration for mapping
            uint16_t cx_min, cx_max, cy_min, cy_max;
            Nav_GetCalibration(&cx_min, &cx_max, &cy_min, &cy_max);

            int32_t nx_i = (cx_max > cx_min)
                ? (int32_t)(ch_h - cx_min) * LCD_WIDTH / (cx_max - cx_min)
                : LCD_WIDTH / 2;
            int32_t cross_h = FOOTER_Y - LIVE_CROSS_Y0;
            int32_t ny_i = LIVE_CROSS_Y0 + (cy_max > cy_min
                ? (int32_t)(cy_max - ch_v) * cross_h / (cy_max - cy_min)
                : cross_h / 2);

            int16_t nx = (nx_i<0)?0:(nx_i>=LCD_WIDTH?LCD_WIDTH-1:(int16_t)nx_i);
            int16_t ny = (ny_i<LIVE_CROSS_Y0)?(int16_t)LIVE_CROSS_Y0
                       : (ny_i>FOOTER_Y-2?(int16_t)(FOOTER_Y-2):(int16_t)ny_i);

            if (live_cx >= 0) live_draw_crosshair(live_cx, live_cy, COL_BG);
            live_draw_crosshair(nx, ny, COL_CROSS);
            live_cx = nx; live_cy = ny;
        }
    }

    if (!pen) { live_prev_pen = false; }
    else       { live_prev_pen = true; }
}

// Returns true if "launch procedure" was requested (right footer button or Enter)
static bool live_wants_proc(void)
{
    // Keyboard Enter
    if (Keyboard_HasNewKey() && Keyboard_GetKeycode() == KB_KEY_ENTER)
        return true;

    // Touch right footer button (check on fresh pen-down)
    if (XPT2046_Read_Pen() == 0 && !live_prev_pen) {
        uint16_t ch_v = XPT2046_Repeated_Compare_AD(CAL_VERT_CMD);
        uint16_t ch_h = XPT2046_Repeated_Compare_AD(CAL_HORIZ_CMD);
        if (ch_v != 0 && ch_h != 0 && check_footer_button(ch_h, ch_v) == 2)
            return true;
    }
    return false;
}

// ===========================================================================
// PROCEDURE MODE
// ===========================================================================

#define CORNER_SZ   64
static const int16_t PROC_CX[4] = { 0, LCD_WIDTH-CORNER_SZ, LCD_WIDTH-CORNER_SZ, 0 };
static const int16_t PROC_CY[4] = { 0, 0, LCD_HEIGHT-CORNER_SZ, LCD_HEIGHT-CORNER_SZ };

#define DOT_SZ   18
#define DOT_GAP  10
#define DOT_ROW_W  (4*DOT_SZ + 3*DOT_GAP)
#define DOT_X0   ((LCD_WIDTH - DOT_ROW_W) / 2)
#define DOT_Y0   ((LCD_HEIGHT - DOT_SZ) / 2)

#define COL_ACTIVE    0xFFE0u   // yellow — blinks
#define COL_DIM       0x2945u   // dark — blink-off
#define COL_DONE      0x07E0u   // green — confirmed
#define COL_INACTIVE  0x18C3u   // dark gray — pending

static uint8_t  proc_step;
static bool     proc_done[4];
static uint16_t proc_min_x, proc_max_x, proc_min_y, proc_max_y;
static bool     proc_blink_on;
static uint32_t proc_last_blink;
static bool     proc_prev_pen;

static void proc_draw_corner(int step, uint16_t col)
{
    int16_t x = PROC_CX[step], y = PROC_CY[step];
    GUI_FillRectColor(x, y, x+CORNER_SZ, y+CORNER_SZ, col);
    int16_t cx = x + CORNER_SZ/2, cy = y + CORNER_SZ/2;
    GUI_SetColor(WHITE);
    GUI_DrawLine(cx-12, cy, cx+12, cy);
    GUI_DrawLine(cx, cy-12, cx, cy+12);
    GUI_FillRectColor(cx-3, cy-3, cx+3, cy+3, WHITE);
}

static void proc_draw_all_corners(void)
{
    for (int i = 0; i < 4; i++) {
        uint16_t col = proc_done[i] ? COL_DONE
                     : (i==(int)proc_step) ? (proc_blink_on?COL_ACTIVE:COL_DIM)
                     : COL_INACTIVE;
        proc_draw_corner(i, col);
    }
}

static void proc_draw_dots(void)
{
    GUI_FillRectColor(DOT_X0-4, DOT_Y0-4, DOT_X0+DOT_ROW_W+4, DOT_Y0+DOT_SZ+4, COL_BG);
    for (int i = 0; i < 4; i++) {
        int16_t dx = DOT_X0 + i*(DOT_SZ+DOT_GAP);
        uint16_t col = proc_done[i] ? COL_DONE
                     : (i==(int)proc_step) ? COL_ACTIVE : COL_INACTIVE;
        GUI_FillRectColor(dx, DOT_Y0, dx+DOT_SZ, DOT_Y0+DOT_SZ, col);
    }
}

static void proc_draw_summary(void)
{
    // Center area between corners
    int16_t ax = CORNER_SZ, ay = CORNER_SZ;
    int16_t aw = LCD_WIDTH  - 2*CORNER_SZ;
    int16_t ah = FOOTER_Y   - 2*CORNER_SZ;
    GUI_FillRectColor(ax, ay, ax+aw, ay+ah, COL_BG);

    int16_t rows_h = 4*(SEG_H+5);
    int16_t sx = ax + (aw - NUM4_W - 14) / 2;
    int16_t sy = ay + (ah - rows_h) / 2;

    uint16_t vals[4]   = { proc_min_x, proc_max_x, proc_min_y, proc_max_y };
    uint16_t colors[4] = { COL_MIN, COL_MAX, COL_MIN, COL_MAX };

    for (int i = 0; i < 4; i++) {
        int16_t ry = sy + i*(SEG_H+5);
        GUI_FillRectColor(sx, ry+4, sx+8, ry+12, colors[i]);
        seg_u16(sx+12, ry, vals[i], colors[i]);
    }
}

static void proc_draw_static(void)
{
    GUI_Clear(COL_BG);
    proc_draw_all_corners();
    proc_draw_dots();
    draw_footer_proc(false);
}

static void proc_do_save(void)
{
    if (proc_min_x >= proc_max_x || proc_min_y >= proc_max_y) return;

    // Flash Save button yellow briefly
    draw_footer_proc(true);
    Delay_ms(150);

    Settings_t s;
    s.touch_x_min = proc_min_x; s.touch_x_max = proc_max_x;
    s.touch_y_min = proc_min_y; s.touch_y_max = proc_max_y;
    Settings_Save(&s);
    Nav_SetCalibration(proc_min_x, proc_max_x, proc_min_y, proc_max_y);

    // Restore button
    draw_footer_proc(false);
}

static void proc_update(uint32_t now_ms)
{
    // Blink current corner
    if (proc_step < 4) {
        bool blink = ((now_ms / 400u) % 2u) == 0u;
        if (blink != proc_blink_on) {
            proc_blink_on = blink;
            uint16_t col = proc_blink_on ? COL_ACTIVE : COL_DIM;
            proc_draw_corner(proc_step, col);
        }
    }

    // Keyboard: Enter = save, ESC handled by demo_app (exits scene)
    if (Keyboard_HasNewKey()) {
        if (Keyboard_GetKeycode() == KB_KEY_ENTER)
            proc_do_save();
    }

    // Touch
    bool pen = (XPT2046_Read_Pen() == 0);
    if (pen && !proc_prev_pen) {
        uint16_t ch_v = XPT2046_Repeated_Compare_AD(CAL_VERT_CMD);
        uint16_t ch_h = XPT2046_Repeated_Compare_AD(CAL_HORIZ_CMD);

        if (ch_v != 0 && ch_h != 0) {
            uint8_t btn = check_footer_button(ch_h, ch_v);

            if (btn == 1) {
                // QUIT — caller switches back to LIVE
            } else if (btn == 2) {
                // SAVE
                proc_do_save();
            } else if (proc_step < 4) {
                // Calibration point
                if (ch_h < proc_min_x) proc_min_x = ch_h;
                if (ch_h > proc_max_x) proc_max_x = ch_h;
                if (ch_v < proc_min_y) proc_min_y = ch_v;
                if (ch_v > proc_max_y) proc_max_y = ch_v;

                proc_done[proc_step] = true;
                proc_draw_corner(proc_step, COL_DONE);
                proc_step++;
                proc_draw_dots();

                if (proc_step >= 4) {
                    // Auto-save and show summary
                    Delay_ms(150);
                    GUI_FillRectColor(0, 0, LCD_WIDTH, LCD_HEIGHT, COL_DONE);
                    Delay_ms(100);
                    proc_draw_static();
                    // All corners already drawn as DONE in proc_draw_static -> proc_draw_all_corners
                    proc_draw_summary();
                }
            }
        }
    }

    proc_prev_pen = pen;
}

// Returns true if "quit to live" was requested (left footer button)
static bool proc_wants_quit(void)
{
    if (!proc_prev_pen && XPT2046_Read_Pen() == 0) {
        uint16_t ch_v = XPT2046_Repeated_Compare_AD(CAL_VERT_CMD);
        uint16_t ch_h = XPT2046_Repeated_Compare_AD(CAL_HORIZ_CMD);
        if (ch_v != 0 && ch_h != 0 && check_footer_button(ch_h, ch_v) == 1)
            return true;
    }
    return false;
}

// ===========================================================================
// Scene public API
// ===========================================================================

static CalibMode_t mode;

void SceneCalib_OnEnter(void)
{
    mode = MODE_LIVE;

    live_raw_x = live_raw_y = 0;
    live_min_x = 0xFFFFu; live_max_x = 0u;
    live_min_y = 0xFFFFu; live_max_y = 0u;
    live_cx = live_cy = -1;
    live_prev_pen = false;

    live_draw_static();
    live_draw_values();
}

void SceneCalib_OnUpdate(uint32_t now_ms)
{
    if (mode == MODE_LIVE) {
        if (live_wants_proc()) {
            // Switch to procedure sub-mode
            mode = MODE_PROC;
            proc_step = 0;
            proc_blink_on = true;
            proc_last_blink = now_ms;
            proc_prev_pen = false;
            proc_min_x = 0xFFFFu; proc_max_x = 0u;
            proc_min_y = 0xFFFFu; proc_max_y = 0u;
            memset(proc_done, 0, sizeof(proc_done));
            proc_draw_static();
            return;
        }
        live_update(now_ms);
    } else {
        if (proc_wants_quit()) {
            // Back to live mode
            mode = MODE_LIVE;
            live_cx = live_cy = -1;
            live_prev_pen = false;
            live_draw_static();
            live_draw_values();
            return;
        }
        proc_update(now_ms);
    }
}

void SceneCalib_OnExit(void)
{
    // Nothing — saves are done explicitly via SAVE button
}
