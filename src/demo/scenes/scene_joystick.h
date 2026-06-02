/**
 * @file    scene_joystick.h
 * @brief   Scene: affichage état manette Mega Drive (driver MEGA9)
 */

#ifndef _SCENE_JOYSTICK_H_
#define _SCENE_JOYSTICK_H_

#include "ui_nav.h"
#include <stdint.h>
#include <stdbool.h>

void SceneJoystick_OnEnter(void);
bool SceneJoystick_OnUpdate(uint32_t now_ms, NavigationEvent_t event);
void SceneJoystick_OnExit(void);

#endif
