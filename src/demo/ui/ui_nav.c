/**
 * @file    ui_nav.c
 * @brief   Unified navigation input — keyboard + touch → abstract events
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#include "ui_nav.h"
#include "keyboard.h"
#include "xpt2046.h"
#include "os_timer.h"
#include "mks_tft28.h"
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Keyboard auto-repeat timing
// ---------------------------------------------------------------------------
#define REPEAT_DELAY_MS  400u   // hold duration before first repeat fires
#define REPEAT_RATE_MS   120u   // interval between subsequent repeats

// ---------------------------------------------------------------------------
// Touch calibration — runtime values, overridable via Nav_SetCalibration()
// ---------------------------------------------------------------------------
#define TOUCH_X_CMD  0xD0   // XPT2046 channel: X axis
#define TOUCH_Y_CMD  0x90   // XPT2046 channel: Y axis

static uint16_t cal_x_min = 200u;
static uint16_t cal_x_max = 3900u;
static uint16_t cal_y_min = 200u;
static uint16_t cal_y_max = 3900u;

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------
static uint8_t  kb_prev_key     = 0;
static uint32_t kb_key_time     = 0;
static bool     kb_repeat_armed = false;

static bool     touch_prev_pen  = false;
static int16_t  touch_last_x    = 0;
static int16_t  touch_last_y    = 0;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static NavEvent_t keycode_to_nav(uint8_t key)
{
    switch (key) {
        case KB_KEY_LEFT:      return NAV_LEFT;
        case KB_KEY_RIGHT:     return NAV_RIGHT;
        case KB_KEY_UP:        return NAV_UP;
        case KB_KEY_DOWN:      return NAV_DOWN;
        case KB_KEY_ENTER:     return NAV_CONFIRM;
        case KB_KEY_ESCAPE:    return NAV_BACK;
        default:               return NAV_NONE;
    }
}

static int16_t clamp16(int32_t v, int16_t lo, int16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return (int16_t)v;
}

static int16_t touch_raw_to_screen_x(uint16_t raw)
{
    if (raw < cal_x_min) raw = cal_x_min;
    if (raw > cal_x_max) raw = cal_x_max;
    int32_t px = (int32_t)(raw - cal_x_min) * LCD_WIDTH
                 / (int32_t)(cal_x_max - cal_x_min);
    return clamp16(px, 0, LCD_WIDTH - 1);
}

static int16_t touch_raw_to_screen_y(uint16_t raw)
{
    if (raw < cal_y_min) raw = cal_y_min;
    if (raw > cal_y_max) raw = cal_y_max;
    int32_t py = (int32_t)(raw - cal_y_min) * LCD_HEIGHT
                 / (int32_t)(cal_y_max - cal_y_min);
    return clamp16(py, 0, LCD_HEIGHT - 1);
}

static NavEvent_t poll_keyboard(uint32_t now)
{
    uint8_t key = Keyboard_GetKeycode();

    if (key == 0) {
        // All keys released
        kb_prev_key     = 0;
        kb_repeat_armed = false;
        return NAV_NONE;
    }

    if (key != kb_prev_key) {
        // New key pressed — immediate event, arm repeat timer
        kb_prev_key     = key;
        kb_key_time     = now;
        kb_repeat_armed = false;
        return keycode_to_nav(key);
    }

    // Same key held — first repeat after REPEAT_DELAY_MS
    if (!kb_repeat_armed) {
        if ((now - kb_key_time) >= REPEAT_DELAY_MS) {
            kb_repeat_armed = true;
            kb_key_time     = now;
            return keycode_to_nav(key);
        }
        return NAV_NONE;
    }

    // Subsequent repeats at REPEAT_RATE_MS
    if ((now - kb_key_time) >= REPEAT_RATE_MS) {
        kb_key_time = now;
        return keycode_to_nav(key);
    }

    return NAV_NONE;
}

static NavEvent_t poll_touch(void)
{
    // XPT2046 TPEN is active-low: 0 = screen touched
    bool pen = (XPT2046_Read_Pen() == 0);

    if (pen && !touch_prev_pen) {
        // Rising edge: new tap
        uint16_t rx = XPT2046_Repeated_Compare_AD(TOUCH_X_CMD);
        uint16_t ry = XPT2046_Repeated_Compare_AD(TOUCH_Y_CMD);

        if (rx != 0 && ry != 0) {
            touch_last_x  = touch_raw_to_screen_x(rx);
            touch_last_y  = touch_raw_to_screen_y(ry);
            touch_prev_pen = true;
            return NAV_TOUCH;
        }
    }

    if (!pen)
        touch_prev_pen = false;

    return NAV_NONE;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Nav_Init(void)
{
    kb_prev_key     = 0;
    kb_key_time     = 0;
    kb_repeat_armed = false;
    touch_prev_pen  = false;
    touch_last_x    = 0;
    touch_last_y    = 0;
}

NavEvent_t Nav_Poll(void)
{
    uint32_t now = OS_GetTimeMs();

    // Keyboard takes priority over touch for navigation
    NavEvent_t ev = poll_keyboard(now);
    if (ev != NAV_NONE) return ev;

    return poll_touch();
}

void Nav_GetTouchPos(int16_t *x, int16_t *y)
{
    if (x) *x = touch_last_x;
    if (y) *y = touch_last_y;
}

void Nav_SetCalibration(uint16_t x_min, uint16_t x_max,
                        uint16_t y_min, uint16_t y_max)
{
    cal_x_min = x_min;
    cal_x_max = x_max;
    cal_y_min = y_min;
    cal_y_max = y_max;
}
