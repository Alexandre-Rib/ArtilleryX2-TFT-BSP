/**
 * @file    scene_image.h
 * @brief   Scene: image viewer — BMP/PNG from external flash or SD card
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#ifndef _SCENE_IMAGE_H_
#define _SCENE_IMAGE_H_

#include <stdint.h>

void SceneImage_OnEnter(void);
void SceneImage_OnUpdate(uint32_t now_ms);
void SceneImage_OnExit(void);

#endif
