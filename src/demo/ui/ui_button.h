/**
 * @file    ui_button.h
 * @brief   Button widget — draw, hit-test, keyboard focus aura
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#ifndef _UI_BUTTON_H_
#define _UI_BUTTON_H_

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Button visual state
// ---------------------------------------------------------------------------

/**
 * @brief  Visual state of a button.
 *
 * Determines background color, border style, and label color when Button_Draw()
 * renders the button.
 */
typedef enum {
    BUTTON_NORMAL   = 0, ///< Default idle state.
    BUTTON_FOCUSED,      ///< Keyboard navigation: cyan focus-ring border.
    BUTTON_PRESSED,      ///< Touch or confirm: lighter background + focus ring.
    BUTTON_DISABLED,     ///< Grayed out, non-interactive placeholder.
} ButtonState_t;

// ---------------------------------------------------------------------------
// Button descriptor
// ---------------------------------------------------------------------------

/**
 * @brief  Button geometry, label, and visual state.
 *
 * Fill the struct once at initialisation.
 * Update only the @c state field at runtime to change appearance.
 */
typedef struct {
    int16_t       x, y;   ///< Top-left corner in screen coordinates (pixels).
    int16_t       w, h;   ///< Width and height in pixels.
    const char   *label;  ///< UTF-8 label, centered inside the button. May be NULL.
    ButtonState_t state;  ///< Current visual state.
} Button_t;

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

/**
 * @brief  Render a button according to its current state.
 *
 * Draws the background fill, border or focus aura, and the centered label.
 * The exact colors are determined by @c button->state.
 *
 * @param[in] button  Button descriptor to render.
 */
void Button_Draw(const Button_t *button);

/**
 * @brief  Test whether a point falls inside a button's bounding box.
 *
 * @param[in] button  Button descriptor.
 * @param[in] x       Screen X coordinate of the point to test.
 * @param[in] y       Screen Y coordinate of the point to test.
 * @return true   The point is inside the button area.
 * @return false  The point is outside the button area.
 */
bool Button_HitTest(const Button_t *button, int16_t x, int16_t y);

#endif
