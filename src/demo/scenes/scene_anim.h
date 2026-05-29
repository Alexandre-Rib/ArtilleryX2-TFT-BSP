/**
 * @file    scene_anim.h
 * @brief   Scene: sprite animation + non-blocking background melody
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#ifndef _SCENE_ANIM_H_
#define _SCENE_ANIM_H_

#include <stdint.h>

void SceneAnim_OnEnter(void);
void SceneAnim_OnUpdate(uint32_t now_ms);
void SceneAnim_OnExit(void);

#endif
