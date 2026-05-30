/**
 * @file    scene_image.c
 * @brief   Scene: image viewer -- BMP from external flash or SD card
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *          Stub -- full implementation pending.
 */

#include "scene_image.h"
#include "GUI.h"
#include "LCD_Colors.h"

void SceneImage_OnEnter(void)
{
    GUI_Clear(BLACK);
    // TODO: show source submenu (Flash / SD), list files, render selection
}

bool SceneImage_OnUpdate(uint32_t now_ms, NavigationEvent_t event)
{
    (void)now_ms;
    (void)event;
    // TODO: handle submenu navigation and image streaming (scanline by scanline)
    return false;
}

void SceneImage_OnExit(void)
{
    // TODO: release any open file handles, reset state
}
