/**
 * @file    ui_nav.h
 * @brief   Navigation input — keyboard and touch events abstracted into a single stream
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Translates two physical input channels into a single NavigationEvent_t:
 *    - Keyboard : LEFT/RIGHT/UP/DOWN arrows with auto-repeat, Enter, Escape
 *    - Touch    : rising-edge tap detection, coordinates in screen pixels
 *
 *  Auto-repeat timings (keyboard only):
 *    - First repeat : REPEAT_DELAY_MS after the initial press (400 ms)
 *    - Subsequent   : every REPEAT_RATE_MS while the key is held (120 ms)
 */

#ifndef _UI_NAV_H_
#define _UI_NAV_H_

#include <stdint.h>

// ---------------------------------------------------------------------------
// Navigation event type
// ---------------------------------------------------------------------------

/**
 * @brief  Abstract input event produced by Navigation_Poll().
 */
typedef enum {
    NAVIGATION_NONE    = 0, ///< No input detected this iteration.
    NAVIGATION_LEFT,        ///< Left arrow key (with auto-repeat).
    NAVIGATION_RIGHT,       ///< Right arrow key (with auto-repeat).
    NAVIGATION_UP,          ///< Up arrow key (with auto-repeat).
    NAVIGATION_DOWN,        ///< Down arrow key (with auto-repeat).
    NAVIGATION_CONFIRM,     ///< Enter key — activates the focused item.
    NAVIGATION_BACK,        ///< Escape key — signals "go back / cancel".
    NAVIGATION_TOUCH,       ///< Touch tap — call Navigation_GetTouchPosition() for coords.
} NavigationEvent_t;

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

/**
 * @brief  Initialise the navigation input system.
 *
 * Resets all keyboard and touch state to idle.
 * Must be called once before the first Navigation_Poll().
 */
void Navigation_Init(void);

/**
 * @brief  Poll for the next navigation input event.
 *
 * Reads the keyboard (with auto-repeat logic) and the touch screen
 * (rising-edge tap).  Keyboard events take priority when both fire
 * in the same iteration.
 *
 * Must be called every main-loop iteration, after Keyboard_Process().
 *
 * @return Detected event, or NAVIGATION_NONE when no input occurred.
 */
NavigationEvent_t Navigation_Poll(void);

/**
 * @brief  Get the screen coordinates of the last detected touch tap.
 *
 * Valid only in the same iteration where Navigation_Poll() returned
 * NAVIGATION_TOUCH.  Coordinates are in screen pixels (0,0 = top-left).
 *
 * @param[out] x  Screen X coordinate.  Ignored when NULL.
 * @param[out] y  Screen Y coordinate.  Ignored when NULL.
 */
void Navigation_GetTouchPosition(int16_t *x, int16_t *y);

/**
 * @brief  Override the touch calibration constants at runtime.
 *
 * Defaults are defined at compile time in ui_nav.c.
 * Call this after Settings_Load() to apply stored calibration.
 *
 * @param[in] x_min  Raw ADC reading at the left edge.
 * @param[in] x_max  Raw ADC reading at the right edge.
 * @param[in] y_min  Raw ADC reading at the top edge.
 * @param[in] y_max  Raw ADC reading at the bottom edge.
 */
void Navigation_SetTouchCalibration(uint16_t x_min, uint16_t x_max,
                                    uint16_t y_min, uint16_t y_max);

/**
 * @brief  Read back the currently active touch calibration constants.
 *
 * @param[out] x_min  Raw ADC reading at the left edge.   Ignored when NULL.
 * @param[out] x_max  Raw ADC reading at the right edge.  Ignored when NULL.
 * @param[out] y_min  Raw ADC reading at the top edge.    Ignored when NULL.
 * @param[out] y_max  Raw ADC reading at the bottom edge. Ignored when NULL.
 */
void Navigation_GetTouchCalibration(uint16_t *x_min, uint16_t *x_max,
                                    uint16_t *y_min, uint16_t *y_max);

#endif
