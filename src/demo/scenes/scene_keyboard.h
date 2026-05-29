/**
 * @file    scene_keyboard.h
 * @brief   Scene: live keyboard display — keycode, modifiers, ASCII char
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#ifndef _SCENE_KEYBOARD_H_
#define _SCENE_KEYBOARD_H_

#include <stdint.h>

void SceneKeyboard_OnEnter(void);
void SceneKeyboard_OnUpdate(uint32_t now_ms);
void SceneKeyboard_OnExit(void);

#endif
