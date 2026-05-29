/**
 * @file    scene_keyboard.c
 * @brief   Scene: live keyboard display — keycode, modifiers, ASCII char
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *          Stub — full implementation pending.
 */

#include "scene_keyboard.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "keyboard.h"

void SceneKeyboard_OnEnter(void)
{
    GUI_Clear(BLACK);
    // TODO: draw header bar, text area, column headers (keycode / mod / char)
}

void SceneKeyboard_OnUpdate(uint32_t now_ms)
{
    (void)now_ms;
    // TODO: on Keyboard_HasNewKey(): read keycode/mod/char, append row,
    //       auto-scroll when text area is full, handle Backspace
}

void SceneKeyboard_OnExit(void)
{
    // nothing to release
}
