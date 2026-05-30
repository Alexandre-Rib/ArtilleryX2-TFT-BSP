/**
 * @file    ui_nav.c
 * @brief   Navigation input — keyboard and touch events abstracted into a single stream
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
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
#define REPEAT_DELAY_MS  400u   // hold duration before the first repeat fires
#define REPEAT_RATE_MS   120u   // interval between subsequent repeats

// ---------------------------------------------------------------------------
// Touch orientation — adjust these three flags to match the panel mounting.
//   TOUCH_SWAP_XY : 1 = ADC channels 0xD0/0x90 are transposed vs screen axes
//   TOUCH_FLIP_X  : 1 = screen X increases as raw X decreases
//   TOUCH_FLIP_Y  : 1 = screen Y increases as raw Y decreases
// ---------------------------------------------------------------------------
#define TOUCH_SWAP_XY  1
#define TOUCH_FLIP_X   0
#define TOUCH_FLIP_Y   1

#define TOUCH_X_CMD  0xD0
#define TOUCH_Y_CMD  0x90

// ---------------------------------------------------------------------------
// Default touch calibration (compile-time fallback)
// Override at runtime with Navigation_SetTouchCalibration() after Settings_Load()
// ---------------------------------------------------------------------------
static uint16_t cal_x_min = 200u;
static uint16_t cal_x_max = 3900u;
static uint16_t cal_y_min = 200u;
static uint16_t cal_y_max = 3900u;

// ---------------------------------------------------------------------------
// Internal state
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

/**
 * @brief  Map a USB HID keycode to the corresponding NavigationEvent_t.
 *
 * Returns NAVIGATION_NONE for any key that has no navigation mapping
 * (printable characters, function keys, etc.).
 *
 * @param[in] keycode  USB HID keycode from Keyboard_GetKeycode().
 * @return Corresponding navigation event, or NAVIGATION_NONE.
 */
static NavigationEvent_t keycode_to_navigation_event(uint8_t keycode)
{
    switch (keycode) {
        case KB_KEY_LEFT:  return NAVIGATION_LEFT;
        case KB_KEY_RIGHT: return NAVIGATION_RIGHT;
        case KB_KEY_UP:    return NAVIGATION_UP;
        case KB_KEY_DOWN:  return NAVIGATION_DOWN;
        case KB_KEY_ENTER: return NAVIGATION_CONFIRM;
        case KB_KEY_ESCAPE:return NAVIGATION_BACK;
        default:           return NAVIGATION_NONE;
    }
}

/**
 * @brief  Clamp a 32-bit value to the [lo, hi] range and cast to int16_t.
 */
static int16_t clamp_to_screen(int32_t value, int16_t lo, int16_t hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return (int16_t)value;
}

/**
 * @brief  Convert a raw ADC X reading to a screen X coordinate.
 */
static int16_t touch_raw_to_screen_x(uint16_t raw)
{
    if (raw < cal_x_min) raw = cal_x_min;
    if (raw > cal_x_max) raw = cal_x_max;
#if TOUCH_FLIP_X
    int32_t pixel = (int32_t)(cal_x_max - raw) * LCD_WIDTH
                    / (int32_t)(cal_x_max - cal_x_min);
#else
    int32_t pixel = (int32_t)(raw - cal_x_min) * LCD_WIDTH
                    / (int32_t)(cal_x_max - cal_x_min);
#endif
    return clamp_to_screen(pixel, 0, LCD_WIDTH - 1);
}

/**
 * @brief  Convert a raw ADC Y reading to a screen Y coordinate.
 */
static int16_t touch_raw_to_screen_y(uint16_t raw)
{
    if (raw < cal_y_min) raw = cal_y_min;
    if (raw > cal_y_max) raw = cal_y_max;
#if TOUCH_FLIP_Y
    int32_t pixel = (int32_t)(cal_y_max - raw) * LCD_HEIGHT
                    / (int32_t)(cal_y_max - cal_y_min);
#else
    int32_t pixel = (int32_t)(raw - cal_y_min) * LCD_HEIGHT
                    / (int32_t)(cal_y_max - cal_y_min);
#endif
    return clamp_to_screen(pixel, 0, LCD_HEIGHT - 1);
}

/**
 * @brief  Poll the keyboard for the next navigation event, with auto-repeat.
 *
 * @param[in] now_ms  Current timestamp from OS_GetTimeMs().
 * @return Navigation event, or NAVIGATION_NONE if nothing happened.
 */
static NavigationEvent_t poll_keyboard(uint32_t now_ms)
{
    uint8_t keycode = Keyboard_GetKeycode();

    if (keycode == 0) {
        kb_prev_key     = 0;
        kb_repeat_armed = false;
        return NAVIGATION_NONE;
    }

    if (keycode != kb_prev_key) {
        // New key pressed — fire immediately and arm the repeat timer
        kb_prev_key     = keycode;
        kb_key_time     = now_ms;
        kb_repeat_armed = false;
        return keycode_to_navigation_event(keycode);
    }

    // Same key held — wait for REPEAT_DELAY_MS before the first repeat
    if (!kb_repeat_armed) {
        if ((now_ms - kb_key_time) >= REPEAT_DELAY_MS) {
            kb_repeat_armed = true;
            kb_key_time     = now_ms;
            return keycode_to_navigation_event(keycode);
        }
        return NAVIGATION_NONE;
    }

    // Subsequent repeats at REPEAT_RATE_MS
    if ((now_ms - kb_key_time) >= REPEAT_RATE_MS) {
        kb_key_time = now_ms;
        return keycode_to_navigation_event(keycode);
    }

    return NAVIGATION_NONE;
}

/**
 * @brief  Poll the touch screen for a rising-edge tap event.
 *
 * @return NAVIGATION_TOUCH on a new tap; NAVIGATION_NONE otherwise.
 */
static NavigationEvent_t poll_touch(void)
{
    bool pen_down = (XPT2046_Read_Pen() == 0);

    if (pen_down && !touch_prev_pen) {
        // Rising edge: first frame the screen is touched
        uint16_t raw_a = XPT2046_Repeated_Compare_AD(TOUCH_X_CMD);
        uint16_t raw_b = XPT2046_Repeated_Compare_AD(TOUCH_Y_CMD);

        if (raw_a != 0 && raw_b != 0) {
#if TOUCH_SWAP_XY
            touch_last_x = touch_raw_to_screen_x(raw_b);
            touch_last_y = touch_raw_to_screen_y(raw_a);
#else
            touch_last_x = touch_raw_to_screen_x(raw_a);
            touch_last_y = touch_raw_to_screen_y(raw_b);
#endif
            touch_prev_pen = true;
            return NAVIGATION_TOUCH;
        }
    }

    if (!pen_down)
        touch_prev_pen = false;

    return NAVIGATION_NONE;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Navigation_Init(void)
{
    kb_prev_key     = 0;
    kb_key_time     = 0;
    kb_repeat_armed = false;
    touch_prev_pen  = false;
    touch_last_x    = 0;
    touch_last_y    = 0;
}

NavigationEvent_t Navigation_Poll(void)
{
    uint32_t now_ms = OS_GetTimeMs();

    // Keyboard takes priority over touch for navigation
    NavigationEvent_t event = poll_keyboard(now_ms);
    if (event != NAVIGATION_NONE) return event;

    return poll_touch();
}

void Navigation_GetTouchPosition(int16_t *x, int16_t *y)
{
    if (x) *x = touch_last_x;
    if (y) *y = touch_last_y;
}

void Navigation_SetTouchCalibration(uint16_t x_min, uint16_t x_max,
                                    uint16_t y_min, uint16_t y_max)
{
    cal_x_min = x_min;
    cal_x_max = x_max;
    cal_y_min = y_min;
    cal_y_max = y_max;
}

void Navigation_GetTouchCalibration(uint16_t *x_min, uint16_t *x_max,
                                    uint16_t *y_min, uint16_t *y_max)
{
    if (x_min) *x_min = cal_x_min;
    if (x_max) *x_max = cal_x_max;
    if (y_min) *y_min = cal_y_min;
    if (y_max) *y_max = cal_y_max;
}
