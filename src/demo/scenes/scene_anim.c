/**
 * @file    scene_anim.c
 * @brief   Scene: sprite animation + non-blocking background melody
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *          Stub -- full implementation pending.
 */

#include "scene_anim.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "buzzer.h"

void SceneAnim_OnEnter(void)
{
    GUI_Clear(BLACK);
    // TODO: load sprite strip from flash, init animation state,
    //       draw sound-toggle button (bottom-right corner)
}

bool SceneAnim_OnUpdate(uint32_t now_ms, NavigationEvent_t event)
{
    (void)now_ms;
    (void)event;
    // TODO: advance sprite frame (delta render), sequence melody notes
    //       via Buzzer_Set() + OS_GetTimeMs() (non-blocking)
    return false;
}

void SceneAnim_OnExit(void)
{
    Buzzer_Stop();
    // TODO: reset animation state
}
