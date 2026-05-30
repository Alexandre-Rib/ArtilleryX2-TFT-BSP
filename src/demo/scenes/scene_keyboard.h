/**
 * @file    scene_keyboard.h
 * @brief   Scene: live keyboard display — keycode, modifier, ASCII character
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#ifndef _SCENE_KEYBOARD_H_
#define _SCENE_KEYBOARD_H_

#include "ui_nav.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Enter the keyboard live-display scene.
 *
 * Clears the screen and draws the static layout (title bar, column headers,
 * log table, footer).
 */
void SceneKeyboard_OnEnter(void);

/**
 * @brief  Update the keyboard scene for one main-loop iteration.
 *
 * Refreshes the live keycode/modifier/char display when the key state changes.
 * Appends one log row per physical key-press event (Keyboard_HasNewKey()).
 * ESC is not consumed — demo_app exits the scene on NAVIGATION_BACK.
 *
 * @param[in] now_ms  Current timestamp from OS_GetTimeMs().
 * @param[in] event   Navigation event from Navigation_Poll().
 * @return Always false — no event is consumed by this scene.
 */
bool SceneKeyboard_OnUpdate(uint32_t now_ms, NavigationEvent_t event);

/**
 * @brief  Exit the keyboard scene.
 *
 * Nothing to release — display is redrawn on the next scene entry.
 */
void SceneKeyboard_OnExit(void);

#endif
