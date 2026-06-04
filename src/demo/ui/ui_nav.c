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
#include "mega9.h"
#include "xpt2046.h"
#include "os_timer.h"
#include "mks_tft28.h"
#include "mouse_cursor.h"
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

static uint8_t  mouse_prev_btn  = 0;

static uint8_t  gp_prev_dir     = 0;
static uint32_t gp_dir_time     = 0;
static bool     gp_repeat_armed = false;

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
 * @brief  Poll the Mega Drive gamepad for navigation events.
 *
 *  Mapping :
 *    C     → NAVIGATION_CONFIRM  (edge-only)
 *    A     → NAVIGATION_BACK     (edge-only)
 *    Croix → NAVIGATION_UP/DOWN/LEFT/RIGHT  (avec auto-repeat)
 *
 * @return Navigation event, or NAVIGATION_NONE.
 */
static NavigationEvent_t poll_gamepad(void)
{
    if (!Mega9_IsConnected()) {
        gp_prev_dir     = 0;
        gp_repeat_armed = false;
        return NAVIGATION_NONE;
    }

    // Edge-triggered: C (confirm), A (back) — pas de repeat
    uint8_t new_btn = Mega9_GetNewButtons();
    if (new_btn & MEGA9_BTN_C) return NAVIGATION_CONFIRM;
    if (new_btn & MEGA9_BTN_A) return NAVIGATION_BACK;

    // Croix directionnelle avec auto-repeat
    uint32_t now_ms = OS_GetTimeMs();
    uint8_t held = Mega9_GetButtons();
    uint8_t dir = held & (MEGA9_BTN_UP | MEGA9_BTN_DOWN | MEGA9_BTN_LEFT | MEGA9_BTN_RIGHT);

    if (dir == 0u) {
        gp_prev_dir     = 0;
        gp_repeat_armed = false;
        return NAVIGATION_NONE;
    }

    if (dir != gp_prev_dir) {
        gp_prev_dir     = dir;
        gp_dir_time     = now_ms;
        gp_repeat_armed = false;
        if (dir & MEGA9_BTN_UP)    return NAVIGATION_UP;
        if (dir & MEGA9_BTN_DOWN)  return NAVIGATION_DOWN;
        if (dir & MEGA9_BTN_LEFT)  return NAVIGATION_LEFT;
        if (dir & MEGA9_BTN_RIGHT) return NAVIGATION_RIGHT;
    }

    if (!gp_repeat_armed) {
        if ((now_ms - gp_dir_time) >= REPEAT_DELAY_MS) {
            gp_repeat_armed = true;
            gp_dir_time     = now_ms;
            if (dir & MEGA9_BTN_UP)    return NAVIGATION_UP;
            if (dir & MEGA9_BTN_DOWN)  return NAVIGATION_DOWN;
            if (dir & MEGA9_BTN_LEFT)  return NAVIGATION_LEFT;
            if (dir & MEGA9_BTN_RIGHT) return NAVIGATION_RIGHT;
        }
        return NAVIGATION_NONE;
    }

    if ((now_ms - gp_dir_time) >= REPEAT_RATE_MS) {
        gp_dir_time = now_ms;
        if (dir & MEGA9_BTN_UP)    return NAVIGATION_UP;
        if (dir & MEGA9_BTN_DOWN)  return NAVIGATION_DOWN;
        if (dir & MEGA9_BTN_LEFT)  return NAVIGATION_LEFT;
        if (dir & MEGA9_BTN_RIGHT) return NAVIGATION_RIGHT;
    }

    return NAVIGATION_NONE;
}

/**
 * @brief  Poll the USB HID mouse for movement and button events.
 *
 *  Calls MouseCursor_MoveDelta() to keep cursor position up-to-date.
 *  Left-button press (rising edge) is translated to NAVIGATION_TOUCH at the
 *  cursor hotspot position, matching touchscreen tap semantics.
 *
 * @return NAVIGATION_TOUCH on LMB press; NAVIGATION_NONE otherwise.
 */
static NavigationEvent_t poll_mouse(void)
{
    if (!Mouse_IsConnected()) {
        mouse_prev_btn = 0;
        return NAVIGATION_NONE;
    }

    int8_t  dx, dy;
    uint8_t buttons;
    Mouse_GetState(&dx, &dy, &buttons);

    if (dx != 0 || dy != 0)
        MouseCursor_MoveDelta(dx, dy);

    uint8_t lmb = buttons & MOUSE_BTN_LEFT;
    NavigationEvent_t ev = NAVIGATION_NONE;

    if (lmb && !mouse_prev_btn) {
        // Rising edge — treat as a touch tap at the cursor hotspot
        MouseCursor_GetPos(&touch_last_x, &touch_last_y);
        ev = NAVIGATION_TOUCH;
    }
    mouse_prev_btn = lmb;
    return ev;
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

void Navigation_FlushKeyboard(void)
{
    kb_prev_key     = 0;
    kb_repeat_armed = false;
    Mega9_FlushEdges();
}

void Navigation_Init(void)
{
    kb_prev_key     = 0;
    kb_key_time     = 0;
    kb_repeat_armed = false;
    touch_prev_pen  = false;
    touch_last_x    = 0;
    touch_last_y    = 0;
    mouse_prev_btn  = 0;
    gp_prev_dir     = 0;
    gp_dir_time     = 0;
    gp_repeat_armed = false;
    MouseCursor_Init();
}

NavigationEvent_t Navigation_Poll(void)
{
    uint32_t now_ms = OS_GetTimeMs();

    // Keyboard takes priority
    NavigationEvent_t event = poll_keyboard(now_ms);
    if (event != NAVIGATION_NONE) return event;

    // Gamepad (same priority as keyboard, above touch)
    event = poll_gamepad();
    if (event != NAVIGATION_NONE) return event;

    // Mouse — updates cursor position and generates synthetic NAVIGATION_TOUCH
    event = poll_mouse();
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
