/**
 * @file    scene_calib.c
 * @brief   Scene: touch calibration -- live display + guided 4-corner procedure
 * @version 5.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Two sub-modes accessed via a footer menu (keyboard + touch):
 *
 *  LIVE mode:
 *    X section (cyan) + Y section (magenta): position bar + MIN/MAX 7-seg display
 *    Crosshair showing calibrated finger position
 *    Footer: [BACK (no action)] [PROCEDURE]
 *
 *  PROCEDURE mode:
 *    4-corner guided calibration TL -> TR -> BR -> BL
 *    Footer: [QUIT] [SAVE (enabled only after all 4 corners tapped)]
 *
 *  ESC key:
 *    In PROCEDURE mode -> consumed, returns to LIVE (on_update returns true)
 *    In LIVE mode      -> not consumed (returns false) -> demo_app exits scene
 *
 *  Touch footer detection:
 *    Uses 10% of the calibration ADC range as threshold -- robust even before
 *    calibration (avoids false positives from an uncalibrated screen).
 *
 *  Entry debounce:
 *    First 250 ms after scene entry: touch input ignored to prevent the tap
 *    that opened this scene from immediately triggering a footer button.
 */

#include "scene_calib.h"
#include "demo_app.h"
#include "settings.h"
#include "ui_menu.h"
#include "ui_nav.h"
#include "font_embedded.h"
#include "xpt2046.h"
#include "keyboard.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "mks_tft28.h"
#include "os_timer.h"
#include "delay.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ---------------------------------------------------------------------------
// ADC measurement commands (XPT2046 protocol)
// ---------------------------------------------------------------------------
#define ADC_CMD_VERTICAL    0xD0
#define ADC_CMD_HORIZONTAL  0x90

// ---------------------------------------------------------------------------
// Sub-modes
// ---------------------------------------------------------------------------
typedef enum { MODE_LIVE, MODE_PROCEDURE } CalibrationMode_t;
static CalibrationMode_t current_mode;

// ---------------------------------------------------------------------------
// Entry debounce -- ignore touch for 250 ms after entering the scene
// ---------------------------------------------------------------------------
#define ENTRY_DEBOUNCE_MS  250u
static uint32_t entry_time_ms;

/**
 * @brief  Returns true once the entry debounce period has elapsed.
 */
static bool touch_debounce_elapsed(void)
{
    return (OS_GetTimeMs() - entry_time_ms) >= ENTRY_DEBOUNCE_MS;
}

// ---------------------------------------------------------------------------
// Shared footer geometry
// ---------------------------------------------------------------------------
#define FOOTER_H   26
#define FOOTER_Y   (LCD_HEIGHT - FOOTER_H)

// ---------------------------------------------------------------------------
// Footer touch detection
//
// Uses 10% of the ADC range as a Y threshold to detect taps in the footer zone,
// even before calibration is accurate.
//
// @param[in] adc_horizontal  Raw horizontal ADC reading.
// @param[in] adc_vertical    Raw vertical ADC reading.
// @return 0 = not in footer zone, 1 = left button, 2 = right button.
// ---------------------------------------------------------------------------
static uint8_t detect_footer_touch(uint16_t adc_horizontal, uint16_t adc_vertical)
{
    uint16_t cal_x_min, cal_x_max, cal_y_min, cal_y_max;
    Navigation_GetTouchCalibration(&cal_x_min, &cal_x_max, &cal_y_min, &cal_y_max);

    // FLIP_Y=1: physical bottom = LOW adc_vertical
    uint16_t footer_threshold = (uint16_t)(cal_y_min + (uint32_t)(cal_y_max - cal_y_min) / 10u);

    if (adc_vertical > footer_threshold) return 0;

    uint16_t horizontal_mid = (uint16_t)(((uint32_t)cal_x_min + cal_x_max) / 2u);
    return (adc_horizontal < horizontal_mid) ? 1u : 2u;
}

// ---------------------------------------------------------------------------
// 7-segment renderer
// Draws each segment ON or OFF to avoid flicker on incremental updates.
// ---------------------------------------------------------------------------
#define SEG_W   12
#define SEG_H   18
#define SEG_T    2
#define SEG_M   (SEG_H / 2)
#define SEG_GAP  3
#define NUM4_W  (4 * SEG_W + 3 * SEG_GAP)

static const uint8_t SEG7[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

/**
 * @brief  Draw one 7-segment digit.
 *
 * @param[in] x    Top-left X of the digit cell.
 * @param[in] y    Top-left Y of the digit cell.
 * @param[in] digit  0-9.
 * @param[in] on_color   Color for lit segments.
 * @param[in] off_color  Color for unlit segments.
 */
static void draw_segment_digit(int16_t x, int16_t y, uint8_t digit,
                                uint16_t on_color, uint16_t off_color)
{
    if (digit > 9) return;
    uint8_t mask = SEG7[digit];
    GUI_FillRectColor(x+SEG_T,       y,              x+SEG_W-SEG_T, y+SEG_T,          (mask&0x01)?on_color:off_color);
    GUI_FillRectColor(x+SEG_W-SEG_T, y,              x+SEG_W,       y+SEG_M,          (mask&0x02)?on_color:off_color);
    GUI_FillRectColor(x+SEG_W-SEG_T, y+SEG_M,        x+SEG_W,       y+SEG_H,          (mask&0x04)?on_color:off_color);
    GUI_FillRectColor(x+SEG_T,       y+SEG_H-SEG_T,  x+SEG_W-SEG_T,y+SEG_H,          (mask&0x08)?on_color:off_color);
    GUI_FillRectColor(x,             y+SEG_M,         x+SEG_T,       y+SEG_H,          (mask&0x10)?on_color:off_color);
    GUI_FillRectColor(x,             y,               x+SEG_T,       y+SEG_M,          (mask&0x20)?on_color:off_color);
    GUI_FillRectColor(x+SEG_T,       y+SEG_M-1,      x+SEG_W-SEG_T,y+SEG_M+SEG_T-1,  (mask&0x40)?on_color:off_color);
}

/**
 * @brief  Draw a 4-digit 7-segment number.
 *
 * @param[in] x          Top-left X.
 * @param[in] y          Top-left Y.
 * @param[in] value      Value to display (0-9999).
 * @param[in] on_color   Color for lit segments.
 * @param[in] off_color  Color for unlit segments.
 */
static void draw_segment_number(int16_t x, int16_t y, uint16_t value,
                                 uint16_t on_color, uint16_t off_color)
{
    int16_t step = SEG_W + SEG_GAP;
    draw_segment_digit(x,         y, (value/1000)%10, on_color, off_color);
    draw_segment_digit(x+step,    y, (value/100)%10,  on_color, off_color);
    draw_segment_digit(x+2*step,  y, (value/10)%10,   on_color, off_color);
    draw_segment_digit(x+3*step,  y, value%10,         on_color, off_color);
}

// ---------------------------------------------------------------------------
// Footer icon drawing helpers
// ---------------------------------------------------------------------------
#define COLOR_BTN_BACK   0x8800u
#define COLOR_BTN_PROC   0x0198u
#define COLOR_BTN_SAVE   0x0320u
#define COLOR_BTN_DIM    0x2104u
#define COLOR_ICON       0xFFFFu

/**
 * @brief  Draw a left-arrow icon centered at (cx, cy).
 */
static void draw_icon_back(int16_t cx, int16_t cy)
{
    GUI_SetColor(COLOR_ICON);
    for (int thickness = 0; thickness <= 1; thickness++) {
        GUI_DrawLine(cx+7+thickness, cy-7, cx-4+thickness, cy);
        GUI_DrawLine(cx-4+thickness, cy,   cx+7+thickness, cy+7);
    }
}

/**
 * @brief  Draw a frame/viewfinder icon centered at (cx, cy).
 */
static void draw_icon_frame(int16_t cx, int16_t cy)
{
    int16_t r = 8;
    GUI_SetColor(COLOR_ICON);
    GUI_DrawLine(cx-r, cy-r, cx-r, cy-r+5);  GUI_DrawLine(cx-r, cy-r, cx-r+5, cy-r);
    GUI_DrawLine(cx+r, cy-r, cx+r, cy-r+5);  GUI_DrawLine(cx+r, cy-r, cx+r-5, cy-r);
    GUI_DrawLine(cx-r, cy+r, cx-r, cy+r-5);  GUI_DrawLine(cx-r, cy+r, cx-r+5, cy+r);
    GUI_DrawLine(cx+r, cy+r, cx+r, cy+r-5);  GUI_DrawLine(cx+r, cy+r, cx+r-5, cy+r);
}

/**
 * @brief  Draw a floppy-disk (save) icon centered at (cx, cy).
 */
static void draw_icon_save(int16_t cx, int16_t cy)
{
    GUI_SetColor(COLOR_ICON);
    GUI_DrawLine(cx,   cy-7, cx,   cy+2);
    GUI_DrawLine(cx-5, cy-2, cx,   cy+4);
    GUI_DrawLine(cx+5, cy-2, cx,   cy+4);
    GUI_DrawLine(cx-7, cy+6, cx+7, cy+6);
    GUI_DrawLine(cx-7, cy+7, cx+7, cy+7);
}

// ---------------------------------------------------------------------------
// Footer custom draw functions (used as MenuItem_t.draw_fn)
// ---------------------------------------------------------------------------
static bool live_focused_0 = false;
static bool live_focused_1 = false;

static void draw_live_back(bool focused)
{
    live_focused_0 = focused;
    int16_t footer_y = FOOTER_Y;
    int16_t mid      = LCD_WIDTH / 2;
    uint16_t color   = focused ? 0xC000u : COLOR_BTN_BACK;
    GUI_FillRectColor(0,   footer_y, mid, LCD_HEIGHT, color);
    draw_icon_back(mid / 2, footer_y + FOOTER_H / 2);
}

static void draw_live_procedure(bool focused)
{
    live_focused_1 = focused;
    int16_t footer_y = FOOTER_Y;
    int16_t mid      = LCD_WIDTH / 2;
    uint16_t color   = focused ? 0x03FFu : COLOR_BTN_PROC;
    GUI_FillRectColor(mid, footer_y, LCD_WIDTH, LCD_HEIGHT, color);
    draw_icon_frame(mid + mid / 2, footer_y + FOOTER_H / 2);
}

static bool procedure_focused_0  = false;
static bool procedure_focused_1  = false;
static bool procedure_save_enabled = false;  // true once all 4 corners are captured

// Procedure footer buttons are constrained to the center zone x=[64..256]
// so they never overlap the 64×64 corner touch targets (CORNER_SZ=64 defined below).
#define PROC_BTN_X0  64               // left edge  = CORNER_SZ
#define PROC_BTN_X1  (LCD_WIDTH / 2)  // split      = 160
#define PROC_BTN_X2  (LCD_WIDTH - 64) // right edge = LCD_WIDTH - CORNER_SZ = 256
#define PROC_BTN_W   96               // half-width = PROC_BTN_X1 - PROC_BTN_X0

static void draw_procedure_quit(bool focused)
{
    procedure_focused_0 = focused;
    uint16_t color = focused ? 0xC000u : COLOR_BTN_BACK;
    GUI_FillRectColor(PROC_BTN_X0, FOOTER_Y, PROC_BTN_X1, LCD_HEIGHT, color);
    draw_icon_back((PROC_BTN_X0 + PROC_BTN_X1) / 2, FOOTER_Y + FOOTER_H/2);
}

static void draw_procedure_save(bool focused)
{
    procedure_focused_1 = focused;
    uint16_t color = !procedure_save_enabled ? COLOR_BTN_DIM
                   : (focused ? 0x07FFu : COLOR_BTN_SAVE);
    GUI_FillRectColor(PROC_BTN_X1, FOOTER_Y, PROC_BTN_X2, LCD_HEIGHT, color);
    draw_icon_save((PROC_BTN_X1 + PROC_BTN_X2) / 2, FOOTER_Y + FOOTER_H/2);
}

// ===========================================================================
// LIVE MODE
// ===========================================================================

#define LIVE_TITLE_H  20
#define LIVE_X_HDR    LIVE_TITLE_H
#define LIVE_X_BAR    (LIVE_X_HDR + 5)
#define LIVE_X_VAL    (LIVE_X_BAR + 14)
#define LIVE_Y_HDR    (LIVE_X_VAL + SEG_H + 6)
#define LIVE_Y_BAR    (LIVE_Y_HDR + 5)
#define LIVE_Y_VAL    (LIVE_Y_BAR + 14)
#define LIVE_CROSS_Y0 (LIVE_Y_VAL + SEG_H + 8)

#define LIVE_COL_BG    0x0000u
#define LIVE_COL_X     0x07FFu
#define LIVE_COL_Y     0xF81Fu
#define LIVE_COL_MIN   0x07E0u
#define LIVE_COL_MAX   0xF800u
#define LIVE_COL_BARB  0x2104u
#define LIVE_COL_CROS  0xFFE0u

#define LIVE_MIN_SW    4
#define LIVE_MIN_VAL   (LIVE_MIN_SW + 10)
#define LIVE_MAX_SW    (LIVE_MIN_VAL + NUM4_W + 14)
#define LIVE_MAX_VAL   (LIVE_MAX_SW + 10)

static uint16_t live_raw_x, live_raw_y;
static uint16_t live_min_x, live_max_x, live_min_y, live_max_y;
static int16_t  live_crosshair_x, live_crosshair_y;
static bool     live_prev_pen;

// Forward declaration for action callback
static void action_live_procedure(void);

static MenuItem_t live_footer_items[2] = {
    { { 0,           FOOTER_Y, LCD_WIDTH/2, FOOTER_H, NULL, BUTTON_NORMAL }, NULL,                draw_live_back      },
    { { LCD_WIDTH/2, FOOTER_Y, LCD_WIDTH/2, FOOTER_H, NULL, BUTTON_NORMAL }, action_live_procedure, draw_live_procedure },
};
static Menu_t live_footer_menu = {
    .items = live_footer_items, .count = 2, .cols = 2, .focused = 0, .parent = NULL
};

/**
 * @brief  Draw the horizontal progress bar for one ADC axis.
 *
 * @param[in] bar_y   Y coordinate of the bar top.
 * @param[in] raw     Raw ADC value (0-4095).
 * @param[in] color   Fill color for the active portion.
 */
static void live_draw_bar(int16_t bar_y, uint16_t raw, uint16_t color)
{
    uint16_t filled = (raw > 0) ? (uint16_t)((uint32_t)raw * (LCD_WIDTH-4) / 4095u) : 0u;
    GUI_FillRectColor(2u, (uint16_t)bar_y, (uint16_t)(2+filled),     (uint16_t)(bar_y+12), color);
    GUI_FillRectColor((uint16_t)(2+filled), (uint16_t)bar_y, (uint16_t)(LCD_WIDTH-2), (uint16_t)(bar_y+12), LIVE_COL_BARB);
}

/**
 * @brief  Draw or erase the crosshair at the given screen position.
 *
 * Clipped to the live zone (between LIVE_CROSS_Y0 and FOOTER_Y - 2).
 *
 * @param[in] x      Screen X.
 * @param[in] y      Screen Y.
 * @param[in] color  Crosshair color, or LIVE_COL_BG to erase.
 */
static void live_draw_crosshair(int16_t x, int16_t y, uint16_t color)
{
    if (y < LIVE_CROSS_Y0 || y > FOOTER_Y - 2) return;
    int16_t r  = 10;
    int16_t x0 = (x-r < 0)            ? 0            : (x-r);
    int16_t x1 = (x+r >= LCD_WIDTH)   ? LCD_WIDTH-1  : (x+r);
    int16_t y0 = (y-r < LIVE_CROSS_Y0)? (int16_t)LIVE_CROSS_Y0 : (y-r);
    int16_t y1 = (y+r > FOOTER_Y-2)   ? (int16_t)(FOOTER_Y-2)  : (y+r);
    GUI_SetColor(color);
    GUI_DrawLine(x0, y, x1, y);
    GUI_DrawLine(x, y0, x, y1);
}

/**
 * @brief  Redraw the X-axis bar and MIN/MAX 7-seg values.
 */
static void live_redraw_x_axis(void)
{
    live_draw_bar(LIVE_X_BAR, live_raw_x, LIVE_COL_X);
    uint16_t display_min = (live_min_x == 0xFFFFu) ? 0u : live_min_x;
    GUI_FillRectColor(LIVE_MIN_SW, LIVE_X_VAL+4, LIVE_MIN_SW+8, LIVE_X_VAL+12, LIVE_COL_MIN);
    draw_segment_number(LIVE_MIN_VAL, LIVE_X_VAL, display_min,    LIVE_COL_MIN, LIVE_COL_BG);
    GUI_FillRectColor(LIVE_MAX_SW, LIVE_X_VAL+4, LIVE_MAX_SW+8, LIVE_X_VAL+12, LIVE_COL_MAX);
    draw_segment_number(LIVE_MAX_VAL, LIVE_X_VAL, live_max_x,    LIVE_COL_MAX, LIVE_COL_BG);
}

/**
 * @brief  Redraw the Y-axis bar and MIN/MAX 7-seg values.
 */
static void live_redraw_y_axis(void)
{
    live_draw_bar(LIVE_Y_BAR, live_raw_y, LIVE_COL_Y);
    uint16_t display_min = (live_min_y == 0xFFFFu) ? 0u : live_min_y;
    GUI_FillRectColor(LIVE_MIN_SW, LIVE_Y_VAL+4, LIVE_MIN_SW+8, LIVE_Y_VAL+12, LIVE_COL_MIN);
    draw_segment_number(LIVE_MIN_VAL, LIVE_Y_VAL, display_min,    LIVE_COL_MIN, LIVE_COL_BG);
    GUI_FillRectColor(LIVE_MAX_SW, LIVE_Y_VAL+4, LIVE_MAX_SW+8, LIVE_Y_VAL+12, LIVE_COL_MAX);
    draw_segment_number(LIVE_MAX_VAL, LIVE_Y_VAL, live_max_y,    LIVE_COL_MAX, LIVE_COL_BG);
}

/**
 * @brief  Draw the complete LIVE mode static layout.
 */
static void live_draw_static_layout(void)
{
    GUI_Clear(LIVE_COL_BG);
    GUI_FillRectColor(0, 0,          LCD_WIDTH, LIVE_TITLE_H,  0x2945u);
    GUI_FillRectColor(0, LIVE_X_HDR, LCD_WIDTH, LIVE_X_HDR+4,  LIVE_COL_X);
    GUI_FillRectColor(0, LIVE_Y_HDR, LCD_WIDTH, LIVE_Y_HDR+4,  LIVE_COL_Y);
    GUI_FillRectColor(0, FOOTER_Y-1, LCD_WIDTH, FOOTER_Y,      0x4228u);

    live_footer_items[0].button.state = BUTTON_FOCUSED;
    live_footer_items[1].button.state = BUTTON_NORMAL;
    Menu_Draw(&live_footer_menu);

    live_redraw_x_axis();
    live_redraw_y_axis();
}

// ===========================================================================
// PROCEDURE MODE
// ===========================================================================

#define CORNER_SZ  64

static const int16_t CORNER_X[4] = { 0, LCD_WIDTH-CORNER_SZ, LCD_WIDTH-CORNER_SZ, 0 };
static const int16_t CORNER_Y[4] = { 0, 0, LCD_HEIGHT-CORNER_SZ, LCD_HEIGHT-CORNER_SZ };

#define DOT_SZ     18
#define DOT_GAP    10
#define DOT_ROW_W  (4*DOT_SZ + 3*DOT_GAP)
#define DOT_X0     ((LCD_WIDTH - DOT_ROW_W) / 2)
#define DOT_Y0     ((FOOTER_Y - CORNER_SZ - DOT_SZ) / 2 + CORNER_SZ)

#define COLOR_STEP_ACTIVE  0xFFE0u
#define COLOR_STEP_DIM     0x2945u
#define COLOR_STEP_DONE    0x07E0u
#define COLOR_STEP_INACTIVE 0x18C3u

static uint8_t  procedure_step;
static bool     procedure_done[4];
static uint16_t procedure_min_x, procedure_max_x, procedure_min_y, procedure_max_y;
static bool     procedure_blink;
static bool     procedure_prev_pen;

// Forward declarations for procedure action callbacks
static void action_procedure_quit(void);
static void action_procedure_save(void);

static MenuItem_t procedure_footer_items[2] = {
    { { PROC_BTN_X0, FOOTER_Y, PROC_BTN_W, FOOTER_H, NULL, BUTTON_NORMAL   }, action_procedure_quit, draw_procedure_quit },
    { { PROC_BTN_X1, FOOTER_Y, PROC_BTN_W, FOOTER_H, NULL, BUTTON_DISABLED }, action_procedure_save, draw_procedure_save },
};
static Menu_t procedure_footer_menu = {
    .items = procedure_footer_items, .count = 2, .cols = 2, .focused = 0, .parent = NULL
};

/**
 * @brief  Draw the target box for one calibration corner.
 *
 * @param[in] step   Corner index (0=TL, 1=TR, 2=BR, 3=BL).
 * @param[in] color  Fill color for the corner box.
 */
static void procedure_draw_corner(int step, uint16_t color)
{
    int16_t x     = CORNER_X[step];
    int16_t y     = CORNER_Y[step];
    int16_t y_end = (y + CORNER_SZ > FOOTER_Y) ? FOOTER_Y : (y + CORNER_SZ);
    GUI_FillRectColor(x, y, x+CORNER_SZ, y_end, color);
    int16_t center_x = x + CORNER_SZ / 2;
    int16_t center_y = (y + y_end) / 2;
    GUI_SetColor(WHITE);
    GUI_DrawLine(center_x-12, center_y, center_x+12, center_y);
    GUI_DrawLine(center_x, center_y-12, center_x, center_y+12);
    GUI_FillRectColor(center_x-3, center_y-3, center_x+3, center_y+3, WHITE);
}

/**
 * @brief  Redraw the progress dots row (one dot per corner, colored by status).
 */
static void procedure_draw_progress_dots(void)
{
    GUI_FillRectColor(DOT_X0-4, DOT_Y0-4, DOT_X0+DOT_ROW_W+4, DOT_Y0+DOT_SZ+4, 0x0000u);
    for (int i = 0; i < 4; i++) {
        int16_t dot_x = DOT_X0 + i*(DOT_SZ+DOT_GAP);
        uint16_t color = procedure_done[i]       ? COLOR_STEP_DONE
                       : (i==(int)procedure_step) ? COLOR_STEP_ACTIVE
                       :                            COLOR_STEP_INACTIVE;
        GUI_FillRectColor(dot_x, DOT_Y0, dot_x+DOT_SZ, DOT_Y0+DOT_SZ, color);
    }
}

/**
 * @brief  Draw the calibration summary (MIN/MAX values) in the centre of the screen.
 */
static void procedure_draw_summary(void)
{
    int16_t area_x = CORNER_SZ;
    int16_t area_y = CORNER_SZ;
    int16_t area_w = LCD_WIDTH  - 2*CORNER_SZ;
    // area_h reduced to FOOTER_Y boundary — use compact row spacing (no gap)
    int16_t area_h = FOOTER_Y - 2*CORNER_SZ;
    GUI_FillRectColor(area_x, area_y, area_x+area_w, area_y+area_h, 0x0000u);

    int16_t  total_h  = 4 * SEG_H;   // compact: no gap between rows (fits in reduced area)
    int16_t  start_x  = area_x + (area_w - NUM4_W - 14) / 2;
    int16_t  start_y  = area_y + (area_h - total_h) / 2;
    uint16_t values[4] = { procedure_min_x, procedure_max_x, procedure_min_y, procedure_max_y };
    uint16_t colors[4] = { LIVE_COL_MIN, LIVE_COL_MAX, LIVE_COL_MIN, LIVE_COL_MAX };

    for (int i = 0; i < 4; i++) {
        int16_t row_y = start_y + i * SEG_H;   // compact row stride
        GUI_FillRectColor(start_x, row_y+4, start_x+8, row_y+12, colors[i]);
        draw_segment_number(start_x+12, row_y, values[i], colors[i], 0x0000u);
    }
}

/**
 * @brief  Enable the SAVE button and move focus to it once all 4 corners are done.
 */
static void procedure_unlock_save(void)
{
    procedure_save_enabled = true;
    procedure_footer_items[1].button.state = BUTTON_NORMAL;
    Menu_Focus(&procedure_footer_menu, 1);
}

/**
 * @brief  Write current procedure calibration values to flash and activate them.
 */
/**
 * @brief  Apply calibration in memory (not persisted).
 *
 * Called immediately after all 4 corners are captured so that the BACK/SAVE
 * footer buttons become touchable with accurate coordinates.
 */
static void apply_calibration(void)
{
    Navigation_SetTouchCalibration(procedure_min_x, procedure_max_x,
                                   procedure_min_y, procedure_max_y);
}

/**
 * @brief  Persist calibration values to flash.
 *
 * Calibration must already be applied via apply_calibration() before calling.
 */
static void save_calibration(void)
{
    if (procedure_min_x >= procedure_max_x || procedure_min_y >= procedure_max_y) return;
    Settings_t settings;
    settings.touch_x_min = procedure_min_x;
    settings.touch_x_max = procedure_max_x;
    settings.touch_y_min = procedure_min_y;
    settings.touch_y_max = procedure_max_y;
    Settings_Save(&settings);
}

/**
 * @brief  Show "SAUVEGARDE OK" in the footer for 2 seconds.
 *
 * Uses the embedded pixel font (MCU flash) — no W25Q64 font data required.
 */
static void show_save_confirmation(void)
{
    GUI_FillRectColor(0, FOOTER_Y, LCD_WIDTH, LCD_HEIGHT, 0x0320u);
    Font_DrawStringCentered(0, FOOTER_Y, LCD_WIDTH, LCD_HEIGHT,
                             "SAVE OK", 2, WHITE);
    Delay_ms(2000);
}

// Action callbacks
static void action_live_procedure(void) { /* transition handled in SceneCalib_OnUpdate */ }
static void action_procedure_quit(void) { /* transition handled in SceneCalib_OnUpdate */ }
static void action_procedure_save(void) { /* handled in SceneCalib_OnUpdate */ }

/**
 * @brief  Draw the complete PROCEDURE mode static layout.
 */
static void procedure_draw_static_layout(void)
{
    GUI_Clear(0x0000u);
    for (int i = 0; i < 4; i++) procedure_draw_corner(i, COLOR_STEP_INACTIVE);
    procedure_draw_corner(0, COLOR_STEP_ACTIVE);
    procedure_draw_progress_dots();

    procedure_save_enabled = false;
    procedure_footer_items[0].button.state = BUTTON_FOCUSED;
    procedure_footer_items[1].button.state = BUTTON_DISABLED;
    procedure_footer_menu.focused = 0;
    // Footer not drawn during corner capture — appears only after step 4 via procedure_unlock_save()
}

/**
 * @brief  Enter PROCEDURE mode from LIVE mode.
 */
static void enter_procedure_mode(void)
{
    current_mode = MODE_PROCEDURE;
    procedure_step     = 0;
    procedure_blink    = true;
    procedure_prev_pen = true;
    procedure_min_x = 0xFFFFu; procedure_max_x = 0u;
    procedure_min_y = 0xFFFFu; procedure_max_y = 0u;
    memset(procedure_done, 0, sizeof(procedure_done));
    procedure_draw_static_layout();
}

/**
 * @brief  Return from PROCEDURE mode to LIVE mode.
 */
static void enter_live_mode(void)
{
    current_mode      = MODE_LIVE;
    live_crosshair_x  = -1;
    live_crosshair_y  = -1;
    live_prev_pen     = true;
    live_footer_menu.focused = 0;
    live_draw_static_layout();
}

// ===========================================================================
// Scene public API
// ===========================================================================

void SceneCalib_OnEnter(void)
{
    current_mode  = MODE_LIVE;
    entry_time_ms = OS_GetTimeMs();

    live_raw_x = live_raw_y = 0;
    live_min_x = 0xFFFFu; live_max_x = 0u;
    live_min_y = 0xFFFFu; live_max_y = 0u;
    live_crosshair_x = live_crosshair_y = -1;
    live_prev_pen = true;   // pre-armed: skip the first touch frame

    live_footer_menu.focused = 0;
    live_draw_static_layout();
}

bool SceneCalib_OnUpdate(uint32_t now_ms, NavigationEvent_t event)
{
    // -----------------------------------------------------------------------
    // PROCEDURE mode
    // -----------------------------------------------------------------------
    if (current_mode == MODE_PROCEDURE) {

        if (event == NAVIGATION_BACK) {
            enter_live_mode();
            return true;
        }

        if (procedure_step < 4) {
            // During capture: keyboard only (touch is reserved for corner capture below)
            NavigationEvent_t keyboard_event = (event == NAVIGATION_TOUCH) ? NAVIGATION_NONE : event;
            MenuResult_t result = Menu_HandleEvent(&procedure_footer_menu, keyboard_event);
            if (result == MENU_RESULT_ACTION && procedure_footer_menu.focused == 0) {
                enter_live_mode();
                return false;
            }
        } else {
            // After all 4 corners: calibration is applied in memory so touch coords
            // are accurate — pass all events including touch to the footer menu.
            MenuResult_t result = Menu_HandleEvent(&procedure_footer_menu, event);
            if (result == MENU_RESULT_ACTION) {
                if (procedure_footer_menu.focused == 1) {
                    // SAVE pressed: persist to flash and show confirmation
                    save_calibration();
                    show_save_confirmation();
                }
                // Both BACK and SAVE return to LIVE
                enter_live_mode();
                return false;
            }
        }

        // Blink the active corner
        if (procedure_step < 4) {
            bool blink_on = ((now_ms / 400u) % 2u) == 0u;
            if (blink_on != procedure_blink) {
                procedure_blink = blink_on;
                procedure_draw_corner(procedure_step, blink_on ? COLOR_STEP_ACTIVE : COLOR_STEP_DIM);
            }
        }

        // Any tap = capture current corner.
        // Footer detection is intentionally skipped here: it relies on calibration
        // values that do not exist yet and would misidentify bottom-screen corners
        // (steps 2 and 3) as footer touches.  QUIT is keyboard-only (ESC key).
        bool pen_down = (XPT2046_Read_Pen() == 0);
        if (pen_down && !procedure_prev_pen && touch_debounce_elapsed() && procedure_step < 4) {
            uint16_t adc_v = XPT2046_Repeated_Compare_AD(ADC_CMD_VERTICAL);
            uint16_t adc_h = XPT2046_Repeated_Compare_AD(ADC_CMD_HORIZONTAL);
            if (adc_v != 0 && adc_h != 0) {
                if (adc_h < procedure_min_x) procedure_min_x = adc_h;
                if (adc_h > procedure_max_x) procedure_max_x = adc_h;
                if (adc_v < procedure_min_y) procedure_min_y = adc_v;
                if (adc_v > procedure_max_y) procedure_max_y = adc_v;
                procedure_done[procedure_step] = true;
                procedure_draw_corner(procedure_step, COLOR_STEP_DONE);
                procedure_step++;
                procedure_draw_progress_dots();
                if (procedure_step >= 4) {
                    Delay_ms(150);
                    GUI_FillRectColor(0, 0, LCD_WIDTH, LCD_HEIGHT, COLOR_STEP_DONE);
                    Delay_ms(100);
                    GUI_Clear(0x0000u);
                    for (int i = 0; i < 4; i++) procedure_draw_corner(i, COLOR_STEP_DONE);
                    procedure_draw_progress_dots();
                    // Apply calibration immediately so the footer buttons are touchable
                    apply_calibration();
                    // Unlock SAVE and redraw footer
                    procedure_unlock_save();
                    procedure_draw_summary();
                }
            }
        }
        procedure_prev_pen = pen_down;
        return false;
    }

    // -----------------------------------------------------------------------
    // LIVE mode
    // -----------------------------------------------------------------------

    // Filter touch from Menu_HandleEvent: without calibration the screen coords
    // are unreliable and would misidentify button presses. Touch is handled
    // separately below via raw ADC (detect_footer_touch).
    NavigationEvent_t keyboard_event = (event == NAVIGATION_TOUCH) ? NAVIGATION_NONE : event;
    MenuResult_t result = Menu_HandleEvent(&live_footer_menu, keyboard_event);
    if (result == MENU_RESULT_BACK) {
        DemoApp_RequestExit();
        return false;
    }
    if (result == MENU_RESULT_ACTION && live_footer_menu.focused == 1) {
        enter_procedure_mode();
        return false;
    }

    bool pen_down = (XPT2046_Read_Pen() == 0);
    if (pen_down && touch_debounce_elapsed()) {
        uint16_t adc_v = XPT2046_Repeated_Compare_AD(ADC_CMD_VERTICAL);
        uint16_t adc_h = XPT2046_Repeated_Compare_AD(ADC_CMD_HORIZONTAL);

        if (adc_v != 0 && adc_h != 0) {
            if (!live_prev_pen) {
                uint8_t footer_btn = detect_footer_touch(adc_h, adc_v);
                if (footer_btn == 1) {
                    DemoApp_RequestExit();
                    live_prev_pen = true;
                    return false;
                }
                if (footer_btn == 2) {
                    enter_procedure_mode();
                    live_prev_pen = true;
                    return false;
                }
            }

            // Update raw values and accumulators
            live_raw_x = adc_h; live_raw_y = adc_v;
            if (adc_h < live_min_x) live_min_x = adc_h;
            if (adc_h > live_max_x) live_max_x = adc_h;
            if (adc_v < live_min_y) live_min_y = adc_v;
            if (adc_v > live_max_y) live_max_y = adc_v;
            live_redraw_x_axis();
            live_redraw_y_axis();

            // Update crosshair position
            uint16_t cal_x_min, cal_x_max, cal_y_min, cal_y_max;
            Navigation_GetTouchCalibration(&cal_x_min, &cal_x_max, &cal_y_min, &cal_y_max);
            int32_t zone_h   = FOOTER_Y - LIVE_CROSS_Y0;
            int32_t screen_x = (cal_x_max > cal_x_min)
                ? (int32_t)(adc_h - cal_x_min) * LCD_WIDTH / (cal_x_max - cal_x_min)
                : LCD_WIDTH / 2;
            int32_t screen_y = (cal_y_max > cal_y_min)
                ? LIVE_CROSS_Y0 + (int32_t)(cal_y_max - adc_v) * zone_h / (cal_y_max - cal_y_min)
                : LIVE_CROSS_Y0 + zone_h / 2;
            int16_t new_x = (screen_x < 0) ? 0
                          : (screen_x >= LCD_WIDTH ? LCD_WIDTH-1 : (int16_t)screen_x);
            int16_t new_y = (screen_y < LIVE_CROSS_Y0) ? (int16_t)LIVE_CROSS_Y0
                          : (screen_y > FOOTER_Y-2     ? (int16_t)(FOOTER_Y-2) : (int16_t)screen_y);

            if (live_crosshair_x >= 0)
                live_draw_crosshair(live_crosshair_x, live_crosshair_y, LIVE_COL_BG);
            live_draw_crosshair(new_x, new_y, LIVE_COL_CROS);
            live_crosshair_x = new_x;
            live_crosshair_y = new_y;
        }
    }
    live_prev_pen = pen_down;

    return false;
}

void SceneCalib_OnExit(void)
{
    // Saves are explicit (SAVE button only) -- nothing to do here
}
