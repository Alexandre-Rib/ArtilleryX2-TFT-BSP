/**
 * @file    scene_image.h
 * @brief   Scene: image viewer — BMP from external flash or SD card
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *          Stub — full implementation pending.
 */

#ifndef _SCENE_IMAGE_H_
#define _SCENE_IMAGE_H_

#include "ui_nav.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Enter the image viewer scene.
 *
 * Shows a source submenu (Flash / SD card) and lists available image files.
 */
void SceneImage_OnEnter(void);

/**
 * @brief  Update the image viewer scene for one main-loop iteration.
 *
 * @param[in] now_ms  Current timestamp from OS_GetTimeMs().
 * @param[in] event   Navigation event from Navigation_Poll().
 * @return true   The event was consumed by this scene.
 * @return false  The event was not consumed; demo_app may act on it (e.g. exit scene).
 */
bool SceneImage_OnUpdate(uint32_t now_ms, NavigationEvent_t event);

/**
 * @brief  Exit the image viewer scene.
 *
 * Releases any open file handles and resets state.
 */
void SceneImage_OnExit(void);

#endif
