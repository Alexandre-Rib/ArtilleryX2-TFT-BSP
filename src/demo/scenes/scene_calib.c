/**
 * @file    scene_calib.c
 * @brief   Scene: touch calibration — live display + guided 4-corner procedure
 * @version 4.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  LIVE mode (entry from menu):
 *    - X section (cyan):   position bar + MIN/MAX 7-seg
 *    - Y section (magenta): position bar + MIN/MAX 7-seg
 *    - Live crosshair in lower area
 *    - Footer: [◄ back (red)] [□ procedure (cyan)]
 *    - Enter key / right footer = launch PROCEDURE sub-mode
 *    - ESC / left footer = back to main menu
 *
 *  PROCEDURE mode (sub-mode):
 *    - Guided 4-corner TL→TR→BR→BL, corner blinks yellow until touched
 *    - Footer: [◄ back to live (red)] [↓ save (green)]
 *    - SAVE button: saves to W25Q64 + confirms, stays in procedure
 *    - QUIT / back = return to LIVE (no save)
 */

#include "scene_calib.h"
#include "settings.h"
#include "ui_nav.h"
#include "xpt2046.h"
#include "keyboard.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "mks_tft28.h"
#include "delay.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ---------------------------------------------------------------------------
// ADC channels
// ---------------------------------------------------------------------------
#define CAL_VERT_CMD   0xD0
#define CAL_HORIZ_CMD  0x90

typedef enum { MODE_LIVE, MODE_PROC } CalibMode_t;
static CalibMode_t mode;

// ---------------------------------------------------------------------------
// Footer geometry (shared)
// ---------------------------------------------------------------------------
#define FOOTER_H   26
#define FOOTER_Y   (LCD_HEIGHT - FOOTER_H)

// ---------------------------------------------------------------------------
// 7-segment renderer — draws each segment ON or OFF (no flicker clear needed)
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

static void seg_digit(int16_t x, int16_t y, uint8_t d, uint16_t col, uint16_t bg)
{
    if (d > 9) return;
    uint8_t m = SEG7[d];
    GUI_FillRectColor(x+SEG_T,       y,              x+SEG_W-SEG_T,  y+SEG_T,            (m&0x01)?col:bg);
    GUI_FillRectColor(x+SEG_W-SEG_T, y,              x+SEG_W,        y+SEG_M,            (m&0x02)?col:bg);
    GUI_FillRectColor(x+SEG_W-SEG_T, y+SEG_M,        x+SEG_W,        y+SEG_H,            (m&0x04)?col:bg);
    GUI_FillRectColor(x+SEG_T,       y+SEG_H-SEG_T,  x+SEG_W-SEG_T, y+SEG_H,            (m&0x08)?col:bg);
    GUI_FillRectColor(x,             y+SEG_M,         x+SEG_T,        y+SEG_H,            (m&0x10)?col:bg);
    GUI_FillRectColor(x,             y,               x+SEG_T,        y+SEG_M,            (m&0x20)?col:bg);
    GUI_FillRectColor(x+SEG_T,       y+SEG_M-1,      x+SEG_W-SEG_T, y+SEG_M+SEG_T-1,   (m&0x40)?col:bg);
}

static void seg_u16(int16_t x, int16_t y, uint16_t v, uint16_t col, uint16_t bg)
{
    int16_t s = SEG_W + SEG_GAP;
    seg_digit(x,     y, (v/1000)%10, col, bg);
    seg_digit(x+s,   y, (v/100)%10,  col, bg);
    seg_digit(x+2*s, y, (v/10)%10,   col, bg);
    seg_digit(x+3*s, y,  v%10,        col, bg);
}

// ---------------------------------------------------------------------------
// Footer button detection — uses current calibration for accurate mapping
// Returns: 0=not footer, 1=left button, 2=right button
// ---------------------------------------------------------------------------
static uint8_t check_footer(uint16_t ch_h, uint16_t ch_v)
{
    uint16_t cx_min, cx_max, cy_min, cy_max;
    Nav_GetCalibration(&cx_min, &cx_max, &cy_min, &cy_max);

    // FLIP_Y=1: physical bottom = low ch_v
    // raw_v threshold for screen Y = FOOTER_Y:
    //   raw = cy_max - FOOTER_Y*(cy_max-cy_min)/H
    int32_t thr = (int32_t)cy_max
                - (int32_t)FOOTER_Y * (cy_max - cy_min) / LCD_HEIGHT;
    if (thr < 0) thr = 0;

    if ((int32_t)ch_v > thr) return 0;   // not in footer

    uint16_t mid = (uint16_t)(((uint32_t)cx_min + cx_max) / 2u);
    return (ch_h < mid) ? 1u : 2u;
}

// ---------------------------------------------------------------------------
// Footer drawing helpers
// ---------------------------------------------------------------------------
#define COL_BTN_BACK  0x8800u   // dark red for back buttons
#define COL_BTN_PROC  0x0198u   // dark teal for procedure / secondary action
#define COL_BTN_SAVE  0x0320u   // dark green for save
#define COL_ICON      0xFFFFu   // white icon

// Back chevron ◄ (left-pointing)
static void icon_back(int16_t cx, int16_t cy)
{
    GUI_SetColor(COL_ICON);
    // Two strokes for thickness
    for (int d = 0; d <= 1; d++) {
        GUI_DrawLine(cx+7+d, cy-7, cx-4+d, cy);
        GUI_DrawLine(cx-4+d, cy,   cx+7+d, cy+7);
    }
}

// Frame corners □ (suggests "enter this zone")
static void icon_frame(int16_t cx, int16_t cy)
{
    int16_t r = 8;
    GUI_SetColor(COL_ICON);
    GUI_DrawLine(cx-r, cy-r, cx-r, cy-r+5);  GUI_DrawLine(cx-r, cy-r, cx-r+5, cy-r);
    GUI_DrawLine(cx+r, cy-r, cx+r, cy-r+5);  GUI_DrawLine(cx+r, cy-r, cx+r-5, cy-r);
    GUI_DrawLine(cx-r, cy+r, cx-r, cy+r-5);  GUI_DrawLine(cx-r, cy+r, cx-r+5, cy+r);
    GUI_DrawLine(cx+r, cy+r, cx+r, cy+r-5);  GUI_DrawLine(cx+r, cy+r, cx+r-5, cy+r);
}

// Save icon: down-arrow + bottom bar
static void icon_save(int16_t cx, int16_t cy)
{
    GUI_SetColor(COL_ICON);
    GUI_DrawLine(cx, cy-7, cx, cy+2);
    GUI_DrawLine(cx-5, cy-2, cx, cy+4);
    GUI_DrawLine(cx+5, cy-2, cx, cy+4);
    GUI_DrawLine(cx-7, cy+6, cx+7, cy+6);
    GUI_DrawLine(cx-7, cy+7, cx+7, cy+7);
}

static void draw_footer(uint16_t lcolor, uint16_t rcolor,
                        void (*licon)(int16_t, int16_t),
                        void (*ricon)(int16_t, int16_t))
{
    int16_t mid = LCD_WIDTH / 2;
    int16_t fy  = FOOTER_Y;
    int16_t cy  = fy + FOOTER_H / 2;

    GUI_FillRectColor(0,   fy, mid,       LCD_HEIGHT, lcolor);
    GUI_FillRectColor(mid, fy, LCD_WIDTH, LCD_HEIGHT, rcolor);
    GUI_FillRectColor(mid-1, fy, mid+1, LCD_HEIGHT, 0x2104u);  // separator

    if (licon) licon(mid / 2,       cy);
    if (ricon) ricon(mid + mid / 2, cy);
}

// ===========================================================================
// LIVE MODE
// ===========================================================================

// Layout: two colour-coded axis sections (X / Y) + crosshair + footer
// No table — each section has its own bar and MIN/MAX values.

#define LIVE_TITLE_H  20    // top title strip
#define LIVE_X_HDR    LIVE_TITLE_H             // X section header (4px color strip)
#define LIVE_X_BAR    (LIVE_X_HDR + 5)         // X position bar
#define LIVE_X_VAL    (LIVE_X_BAR + 14)        // X min/max 7-seg row
#define LIVE_Y_HDR    (LIVE_X_VAL + SEG_H + 6) // Y section header
#define LIVE_Y_BAR    (LIVE_Y_HDR + 5)
#define LIVE_Y_VAL    (LIVE_Y_BAR + 14)
#define LIVE_CROSS_Y0 (LIVE_Y_VAL + SEG_H + 8) // crosshair area starts here

#define LIVE_COL_BG   0x0000u
#define LIVE_COL_X    0x07FFu   // cyan  — X axis
#define LIVE_COL_Y    0xF81Fu   // magenta — Y axis
#define LIVE_COL_MIN  0x07E0u   // green
#define LIVE_COL_MAX  0xF800u   // red
#define LIVE_COL_BARB 0x2104u   // bar background
#define LIVE_COL_CROS 0xFFE0u   // crosshair yellow

// Column positions for min/max within a section
// [MIN swatch 8px][gap 4px][7seg NUM4_W][gap 20px][MAX swatch 8px][gap 4px][7seg]
#define LIVE_MIN_SW  4
#define LIVE_MIN_VAL (LIVE_MIN_SW + 10)
#define LIVE_MAX_SW  (LIVE_MIN_VAL + NUM4_W + 14)
#define LIVE_MAX_VAL (LIVE_MAX_SW + 10)

static uint16_t live_raw_x, live_raw_y;
static uint16_t live_min_x, live_max_x, live_min_y, live_max_y;
static int16_t  live_cx, live_cy;
static bool     live_prev_pen;

static void live_draw_bar(int16_t by, uint16_t raw, uint16_t col)
{
    uint16_t w = (raw > 0) ? (uint16_t)((uint32_t)raw*(LCD_WIDTH-4)/4095u) : 0u;
    GUI_FillRectColor(2u, (uint16_t)by, (uint16_t)(2+w),       (uint16_t)(by+12), col);
    GUI_FillRectColor((uint16_t)(2+w), (uint16_t)by, (uint16_t)(LCD_WIDTH-2), (uint16_t)(by+12), LIVE_COL_BARB);
}

static void live_draw_crosshair(int16_t x, int16_t y, uint16_t col)
{
    if (y < LIVE_CROSS_Y0 || y > FOOTER_Y - 2) return;
    int16_t r = 10;
    int16_t x0=(x-r<0)?0:(x-r), x1=(x+r>=LCD_WIDTH)?LCD_WIDTH-1:(x+r);
    int16_t y0=(y-r<LIVE_CROSS_Y0)?(int16_t)LIVE_CROSS_Y0:(y-r);
    int16_t y1=(y+r>FOOTER_Y-2)?(int16_t)(FOOTER_Y-2):(y+r);
    GUI_SetColor(col);
    GUI_DrawLine(x0, y, x1, y);
    GUI_DrawLine(x, y0, x, y1);
}

// Update values in place — no clear needed (7-seg draws OFF segments as bg)
static void live_update_x(void)
{
    live_draw_bar(LIVE_X_BAR, live_raw_x, LIVE_COL_X);

    uint16_t mn = (live_min_x == 0xFFFFu) ? 0u : live_min_x;
    GUI_FillRectColor(LIVE_MIN_SW, LIVE_X_VAL+4, LIVE_MIN_SW+8, LIVE_X_VAL+12, LIVE_COL_MIN);
    seg_u16(LIVE_MIN_VAL, LIVE_X_VAL, mn,          LIVE_COL_MIN, LIVE_COL_BG);
    GUI_FillRectColor(LIVE_MAX_SW, LIVE_X_VAL+4, LIVE_MAX_SW+8, LIVE_X_VAL+12, LIVE_COL_MAX);
    seg_u16(LIVE_MAX_VAL, LIVE_X_VAL, live_max_x, LIVE_COL_MAX, LIVE_COL_BG);
}

static void live_update_y(void)
{
    live_draw_bar(LIVE_Y_BAR, live_raw_y, LIVE_COL_Y);

    uint16_t mn = (live_min_y == 0xFFFFu) ? 0u : live_min_y;
    GUI_FillRectColor(LIVE_MIN_SW, LIVE_Y_VAL+4, LIVE_MIN_SW+8, LIVE_Y_VAL+12, LIVE_COL_MIN);
    seg_u16(LIVE_MIN_VAL, LIVE_Y_VAL, mn,          LIVE_COL_MIN, LIVE_COL_BG);
    GUI_FillRectColor(LIVE_MAX_SW, LIVE_Y_VAL+4, LIVE_MAX_SW+8, LIVE_Y_VAL+12, LIVE_COL_MAX);
    seg_u16(LIVE_MAX_VAL, LIVE_Y_VAL, live_max_y, LIVE_COL_MAX, LIVE_COL_BG);
}

static void live_draw_static(void)
{
    GUI_Clear(LIVE_COL_BG);

    // Title strip
    GUI_FillRectColor(0, 0, LCD_WIDTH, LIVE_TITLE_H, 0x2945u);

    // X section header (cyan stripe)
    GUI_FillRectColor(0, LIVE_X_HDR, LCD_WIDTH, LIVE_X_HDR+4, LIVE_COL_X);
    // Y section header (magenta stripe)
    GUI_FillRectColor(0, LIVE_Y_HDR, LCD_WIDTH, LIVE_Y_HDR+4, LIVE_COL_Y);

    // Crosshair area corner marks
    GUI_SetColor(0x4228u);
    GUI_DrawLine(0, LIVE_CROSS_Y0, 8, LIVE_CROSS_Y0);
    GUI_DrawLine(0, LIVE_CROSS_Y0, 0, LIVE_CROSS_Y0+8);
    GUI_DrawLine(LCD_WIDTH-8, LIVE_CROSS_Y0, LCD_WIDTH-1, LIVE_CROSS_Y0);
    GUI_DrawLine(LCD_WIDTH-1, LIVE_CROSS_Y0, LCD_WIDTH-1, LIVE_CROSS_Y0+8);

    GUI_FillRectColor(0, FOOTER_Y-1, LCD_WIDTH, FOOTER_Y, 0x4228u);
    draw_footer(COL_BTN_BACK, COL_BTN_PROC, icon_back, icon_frame);

    live_update_x();
    live_update_y();
}

static bool live_update_tick(void)
{
    // Returns true = wants to switch to PROCEDURE
    bool pen = (XPT2046_Read_Pen() == 0);

    // Keyboard: Enter = launch procedure
    if (Keyboard_HasNewKey() && Keyboard_GetKeycode() == KB_KEY_ENTER)
        return true;

    if (pen) {
        uint16_t ch_v = XPT2046_Repeated_Compare_AD(CAL_VERT_CMD);
        uint16_t ch_h = XPT2046_Repeated_Compare_AD(CAL_HORIZ_CMD);

        if (ch_v != 0 && ch_h != 0) {
            if (!live_prev_pen) {
                uint8_t btn = check_footer(ch_h, ch_v);
                if (btn == 2) { live_prev_pen = true; return true; }
                if (btn == 1) { live_prev_pen = true; return false; }
                // btn == 0: calibration data below
            }

            live_raw_x = ch_h; live_raw_y = ch_v;
            if (ch_h < live_min_x) live_min_x = ch_h;
            if (ch_h > live_max_x) live_max_x = ch_h;
            if (ch_v < live_min_y) live_min_y = ch_v;
            if (ch_v > live_max_y) live_max_y = ch_v;

            live_update_x();
            live_update_y();

            // Crosshair
            uint16_t cx_min, cx_max, cy_min, cy_max;
            Nav_GetCalibration(&cx_min, &cx_max, &cy_min, &cy_max);
            int32_t cross_h = FOOTER_Y - LIVE_CROSS_Y0;
            int32_t nx_i = (cx_max > cx_min)
                ? (int32_t)(ch_h - cx_min) * LCD_WIDTH / (cx_max - cx_min)
                : LCD_WIDTH / 2;
            int32_t ny_i = (cy_max > cy_min)
                ? LIVE_CROSS_Y0 + (int32_t)(cy_max - ch_v) * cross_h / (cy_max - cy_min)
                : LIVE_CROSS_Y0 + cross_h / 2;

            int16_t nx = (nx_i<0)?0:(nx_i>=LCD_WIDTH?LCD_WIDTH-1:(int16_t)nx_i);
            int16_t ny = (ny_i<LIVE_CROSS_Y0)?(int16_t)LIVE_CROSS_Y0
                       : (ny_i>FOOTER_Y-2?(int16_t)(FOOTER_Y-2):(int16_t)ny_i);

            if (live_cx >= 0) live_draw_crosshair(live_cx, live_cy, LIVE_COL_BG);
            live_draw_crosshair(nx, ny, LIVE_COL_CROS);
            live_cx = nx; live_cy = ny;
        }
    }

    if (!pen) live_prev_pen = false;
    else      live_prev_pen = true;

    return false;
}

// ===========================================================================
// PROCEDURE MODE
// ===========================================================================

#define CORNER_SZ  64
static const int16_t CX[4] = { 0, LCD_WIDTH-CORNER_SZ, LCD_WIDTH-CORNER_SZ, 0 };
static const int16_t CY[4] = { 0, 0, LCD_HEIGHT-CORNER_SZ, LCD_HEIGHT-CORNER_SZ };

#define DOT_SZ   18
#define DOT_GAP  10
#define DOT_ROW_W  (4*DOT_SZ + 3*DOT_GAP)
#define DOT_X0   ((LCD_WIDTH  - DOT_ROW_W) / 2)
#define DOT_Y0   ((FOOTER_Y   - CORNER_SZ - DOT_SZ) / 2 + CORNER_SZ)

#define COL_PACTIVE   0xFFE0u   // yellow blink
#define COL_PDIM      0x2945u
#define COL_PDONE     0x07E0u   // green
#define COL_PINACT    0x18C3u

static uint8_t  proc_step;
static bool     proc_done[4];
static uint16_t proc_min_x, proc_max_x, proc_min_y, proc_max_y;
static bool     proc_blink;
static bool     proc_prev_pen;

static void proc_corner(int s, uint16_t col)
{
    int16_t x = CX[s], y = CY[s];
    // Clip bottom corners to FOOTER_Y — no shared pixels with footer buttons
    int16_t y_end = (y + CORNER_SZ > FOOTER_Y) ? FOOTER_Y : (y + CORNER_SZ);
    GUI_FillRectColor(x, y, x+CORNER_SZ, y_end, col);
    // Crosshair clamped inside the drawn area
    int16_t cx = x + CORNER_SZ / 2;
    int16_t cy = (y + y_end) / 2;
    GUI_SetColor(WHITE);
    GUI_DrawLine(cx-12, cy, cx+12, cy);
    GUI_DrawLine(cx, cy-12, cx, cy+12);
    GUI_FillRectColor(cx-3, cy-3, cx+3, cy+3, WHITE);
}

static void proc_dots(void)
{
    GUI_FillRectColor(DOT_X0-4, DOT_Y0-4, DOT_X0+DOT_ROW_W+4, DOT_Y0+DOT_SZ+4, 0x0000u);
    for (int i = 0; i < 4; i++) {
        int16_t dx = DOT_X0 + i*(DOT_SZ+DOT_GAP);
        uint16_t col = proc_done[i] ? COL_PDONE
                     : (i==(int)proc_step) ? COL_PACTIVE : COL_PINACT;
        GUI_FillRectColor(dx, DOT_Y0, dx+DOT_SZ, DOT_Y0+DOT_SZ, col);
    }
}

static void proc_summary(void)
{
    int16_t ax=CORNER_SZ, ay=CORNER_SZ;
    int16_t aw=LCD_WIDTH-2*CORNER_SZ, ah=FOOTER_Y-2*CORNER_SZ;
    GUI_FillRectColor(ax, ay, ax+aw, ay+ah, 0x0000u);
    int16_t sh = 4*(SEG_H+5);
    int16_t sx = ax+(aw-NUM4_W-14)/2, sy = ay+(ah-sh)/2;
    uint16_t vals[4]   = {proc_min_x, proc_max_x, proc_min_y, proc_max_y};
    uint16_t cols[4]   = {LIVE_COL_MIN, LIVE_COL_MAX, LIVE_COL_MIN, LIVE_COL_MAX};
    for (int i=0;i<4;i++) {
        int16_t ry = sy + i*(SEG_H+5);
        GUI_FillRectColor(sx, ry+4, sx+8, ry+12, cols[i]);
        seg_u16(sx+12, ry, vals[i], cols[i], 0x0000u);
    }
}

// Footer for procedure: buttons confined to center zone (CORNER_SZ..LCD_WIDTH-CORNER_SZ)
// so they never share pixels with the corner touch targets.
// SAVE only appears once all 4 corners are captured.
static void proc_draw_footer(bool save_flash)
{
    int16_t fy   = FOOTER_Y;
    int16_t mid  = LCD_WIDTH / 2;
    int16_t lx0  = CORNER_SZ;          // left button start  (= 64)
    int16_t rx1  = LCD_WIDTH - CORNER_SZ; // right button end  (= 256)
    int16_t cy   = fy + FOOTER_H / 2;

    // Outer zones (align with corner columns) — neutral black
    GUI_FillRectColor(0,   fy, lx0, LCD_HEIGHT, 0x0000u);
    GUI_FillRectColor(rx1, fy, LCD_WIDTH, LCD_HEIGHT, 0x0000u);

    // Left center: QUIT / back
    GUI_FillRectColor(lx0, fy, mid, LCD_HEIGHT, COL_BTN_BACK);
    icon_back((lx0 + mid) / 2, cy);

    // Right center: SAVE — only when all 4 corners are done
    if (proc_step >= 4) {
        uint16_t col = save_flash ? COL_PACTIVE : COL_BTN_SAVE;
        GUI_FillRectColor(mid, fy, rx1, LCD_HEIGHT, col);
        icon_save((mid + rx1) / 2, cy);
    } else {
        GUI_FillRectColor(mid, fy, rx1, LCD_HEIGHT, 0x2104u);  // dark gray — inactive
    }
}

static void proc_save(void)
{
    if (proc_min_x >= proc_max_x || proc_min_y >= proc_max_y) return;

    // Yellow flash on Save button as confirmation
    proc_draw_footer(true);
    Delay_ms(200);

    Settings_t s;
    s.touch_x_min=proc_min_x; s.touch_x_max=proc_max_x;
    s.touch_y_min=proc_min_y; s.touch_y_max=proc_max_y;
    Settings_Save(&s);
    Nav_SetCalibration(proc_min_x, proc_max_x, proc_min_y, proc_max_y);

    proc_draw_footer(false);
}

static bool proc_update_tick(uint32_t now_ms)
{
    // Returns true = wants to quit back to LIVE

    // Blink current corner
    if (proc_step < 4) {
        bool blink = ((now_ms / 400u) % 2u) == 0u;
        if (blink != proc_blink) {
            proc_blink = blink;
            proc_corner(proc_step, blink ? COL_PACTIVE : COL_PDIM);
        }
    }

    // Keyboard: Enter = save (only when complete)
    if (proc_step >= 4 && Keyboard_HasNewKey() && Keyboard_GetKeycode() == KB_KEY_ENTER)
        proc_save();

    bool pen = (XPT2046_Read_Pen() == 0);
    if (pen && !proc_prev_pen) {
        uint16_t ch_v = XPT2046_Repeated_Compare_AD(CAL_VERT_CMD);
        uint16_t ch_h = XPT2046_Repeated_Compare_AD(CAL_HORIZ_CMD);
        if (ch_v != 0 && ch_h != 0) {
            uint8_t btn = check_footer(ch_h, ch_v);
            if (btn == 1) { proc_prev_pen = true; return true; }         // QUIT
            if (btn == 2 && proc_step >= 4) { proc_save(); }             // SAVE (only when done)
            else if (btn == 0 && proc_step < 4) {
                // Capture corner
                if (ch_h < proc_min_x) proc_min_x = ch_h;
                if (ch_h > proc_max_x) proc_max_x = ch_h;
                if (ch_v < proc_min_y) proc_min_y = ch_v;
                if (ch_v > proc_max_y) proc_max_y = ch_v;
                proc_done[proc_step] = true;
                proc_corner(proc_step, COL_PDONE);
                proc_step++;
                proc_dots();
                proc_draw_footer(false);  // refresh footer — SAVE appears on step 4
                if (proc_step >= 4) {
                    // Auto-save + summary
                    Delay_ms(150);
                    GUI_FillRectColor(0, 0, LCD_WIDTH, LCD_HEIGHT, COL_PDONE);
                    Delay_ms(100);
                    GUI_Clear(0x0000u);
                    for (int i=0;i<4;i++) proc_corner(i, COL_PDONE);
                    proc_dots();
                    proc_draw_footer(false);
                    proc_save();
                    proc_summary();
                }
            }
        }
    }
    if (!pen) proc_prev_pen = false;
    else      proc_prev_pen = true;
    return false;
}

// ===========================================================================
// Scene public API
// ===========================================================================

void SceneCalib_OnEnter(void)
{
    mode = MODE_LIVE;
    live_raw_x = live_raw_y = 0;
    live_min_x = 0xFFFFu; live_max_x = 0u;
    live_min_y = 0xFFFFu; live_max_y = 0u;
    live_cx = live_cy = -1;
    live_prev_pen = false;
    live_draw_static();
}

void SceneCalib_OnUpdate(uint32_t now_ms)
{
    if (mode == MODE_LIVE) {
        if (live_update_tick()) {
            mode = MODE_PROC;
            proc_step = 0; proc_blink = true; proc_prev_pen = false;
            proc_min_x = 0xFFFFu; proc_max_x = 0u;
            proc_min_y = 0xFFFFu; proc_max_y = 0u;
            memset(proc_done, 0, sizeof(proc_done));
            GUI_Clear(0x0000u);
            for (int i=0;i<4;i++) proc_corner(i, COL_PINACT);
            proc_corner(0, COL_PACTIVE);
            proc_dots();
            proc_draw_footer(false);  // SAVE hidden until all 4 corners done
        }
    } else {
        if (proc_update_tick(now_ms)) {
            mode = MODE_LIVE;
            live_cx = live_cy = -1;
            live_prev_pen = false;
            live_draw_static();
        }
    }
}

void SceneCalib_OnExit(void)
{
    // Saves are explicit (SAVE button) — nothing to do here
}
