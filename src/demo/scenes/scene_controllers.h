/**
 * @file    scene_controllers.h
 * @brief   Scene: sous-menu Controllers — clavier / souris / joystick
 */

#ifndef _SCENE_CONTROLLERS_H_
#define _SCENE_CONTROLLERS_H_

#include "ui_nav.h"
#include <stdint.h>
#include <stdbool.h>

void SceneControllers_OnEnter(void);
bool SceneControllers_OnUpdate(uint32_t now_ms, NavigationEvent_t event);
void SceneControllers_OnExit(void);

#endif
