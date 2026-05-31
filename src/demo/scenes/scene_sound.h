/**
 * @file    scene_sound.h
 * @brief   Scene: tone-sequence player — built-in (Flash) or .snd files (SD)
 * @version 1.0
 * @date    Created: 2026-05-31
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#ifndef _SCENE_SOUND_H_
#define _SCENE_SOUND_H_

#include "ui_nav.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Enter the sound player scene.
 *
 * Mounts SD, scans for .snd files, draws the full layout.
 * Starts with Flash source selected; no melody playing.
 */
void SceneSound_OnEnter(void);

/**
 * @brief  Update the sound scene for one main-loop iteration.
 *
 * Ticks the non-blocking tone sequencer (Buzzer_Set + OS_GetTimeMs).
 * Handles source switching (LEFT/RIGHT), list navigation (UP/DOWN),
 * play/stop toggle (CONFIRM), and touch hit-testing.
 * ESC is not consumed — demo_app exits the scene on NAVIGATION_BACK.
 *
 * @param[in] now_ms  Current timestamp from OS_GetTimeMs().
 * @param[in] event   Navigation event from Navigation_Poll().
 * @return Always false — NAVIGATION_BACK propagates to demo_app.
 */
bool SceneSound_OnUpdate(uint32_t now_ms, NavigationEvent_t event);

/**
 * @brief  Exit the sound scene.
 *
 * Stops any active playback and unmounts the SD filesystem.
 */
void SceneSound_OnExit(void);

#endif /* _SCENE_SOUND_H_ */
