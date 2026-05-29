/**
 * @file    ui_nav.h
 * @brief   Unified navigation input — keyboard + touch → abstract events
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Keyboard: arrow keys with auto-repeat, Enter = CONFIRM, Escape = BACK.
 *  Touch:    rising-edge tap detection; caller retrieves coords via
 *            Nav_GetTouchPos() after receiving NAV_TOUCH.
 *
 *  Calibration constants (TOUCH_X_MIN/MAX, TOUCH_Y_MIN/MAX) are defined
 *  in ui_nav.c and may need per-device adjustment.
 */

#ifndef _UI_NAV_H_
#define _UI_NAV_H_

#include <stdint.h>

typedef enum {
    NAV_NONE = 0,
    NAV_LEFT,
    NAV_RIGHT,
    NAV_UP,
    NAV_DOWN,
    NAV_CONFIRM,   // Enter key or tap on focused button
    NAV_BACK,      // Escape key or dedicated back button
    NAV_TOUCH,     // raw touch tap — call Nav_GetTouchPos() for coords
} NavEvent_t;

// Initialise internal state; call once before the main loop
void Nav_Init(void);

// Poll for the next input event; returns NAV_NONE when nothing happened.
// Must be called every main-loop iteration after Keyboard_Process().
NavEvent_t Nav_Poll(void);

// Valid only in the same iteration that Nav_Poll() returned NAV_TOUCH
void Nav_GetTouchPos(int16_t *x, int16_t *y);

// Override the touch calibration constants at runtime.
// Call after Settings_Load() to apply saved calibration.
void Nav_SetCalibration(uint16_t x_min, uint16_t x_max,
                        uint16_t y_min, uint16_t y_max);

// Read back the current calibration constants.
void Nav_GetCalibration(uint16_t *x_min, uint16_t *x_max,
                        uint16_t *y_min, uint16_t *y_max);

#endif
