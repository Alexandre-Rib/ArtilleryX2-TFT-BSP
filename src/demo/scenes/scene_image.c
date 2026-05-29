/**
 * @file    scene_image.c
 * @brief   Scene: image viewer — BMP/PNG from external flash or SD card
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *          Stub — full implementation pending.
 */

#include "scene_image.h"
#include "GUI.h"
#include "LCD_Colors.h"

void SceneImage_OnEnter(void)
{
    GUI_Clear(BLACK);
    // TODO: show source submenu (Flash / SD), list files, render selection
}

void SceneImage_OnUpdate(uint32_t now_ms)
{
    (void)now_ms;
    // TODO: handle submenu navigation and image streaming (scanline by scanline)
}

void SceneImage_OnExit(void)
{
    // TODO: release any open file handles / reset state
}
