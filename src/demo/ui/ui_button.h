/**
 * @file    ui_button.h
 * @brief   Button widget — draw, hit-test, keyboard focus aura
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#ifndef _UI_BUTTON_H_
#define _UI_BUTTON_H_

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Button visual states
// ---------------------------------------------------------------------------
typedef enum {
    BTN_NORMAL = 0,   // default idle state
    BTN_FOCUSED,      // keyboard navigation: cyan aura border
    BTN_PRESSED,      // touch/confirm: lighter background + aura
    BTN_DISABLED,     // placeholder: grayed out, non-interactive
} BtnState_t;

// ---------------------------------------------------------------------------
// Button descriptor — fill once at init, update .state at runtime
// ---------------------------------------------------------------------------
typedef struct {
    int16_t     x, y;      // top-left corner (screen coords)
    int16_t     w, h;      // width and height in pixels
    const char *label;     // UTF-8 label (centered inside button)
    BtnState_t  state;
} Button_t;

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

// Render a button according to its current state
void Button_Draw(const Button_t *btn);

// Returns true if (tx, ty) falls inside the button bounds
bool Button_HitTest(const Button_t *btn, int16_t tx, int16_t ty);

#endif
