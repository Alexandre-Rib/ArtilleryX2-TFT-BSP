/**
 * @file    scene_mouse.h
 * @brief   Scene: affichage souris USB HID (placeholder — non implémenté)
 */

#ifndef _SCENE_MOUSE_H_
#define _SCENE_MOUSE_H_

#include "ui_nav.h"
#include <stdint.h>
#include <stdbool.h>

void SceneMouse_OnEnter(void);
bool SceneMouse_OnUpdate(uint32_t now_ms, NavigationEvent_t event);
void SceneMouse_OnExit(void);

#endif
