/**
 * @file    scene_anim.h
 * @brief   Scene: sprite animation + non-blocking background melody
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *          Stub — full implementation pending.
 */

#ifndef _SCENE_ANIM_H_
#define _SCENE_ANIM_H_

#include "ui_nav.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Enter the animation scene.
 *
 * Loads sprite strip from external flash, initialises animation state,
 * and draws the initial frame.
 */
void SceneAnim_OnEnter(void);

/**
 * @brief  Update the animation scene for one main-loop iteration.
 *
 * @param[in] now_ms  Current timestamp from OS_GetTimeMs().
 * @param[in] event   Navigation event from Navigation_Poll().
 * @return true   The event was consumed by this scene.
 * @return false  The event was not consumed; demo_app may act on it (e.g. exit scene).
 */
bool SceneAnim_OnUpdate(uint32_t now_ms, NavigationEvent_t event);

/**
 * @brief  Exit the animation scene.
 *
 * Stops the buzzer and resets animation state.
 */
void SceneAnim_OnExit(void);

#endif
