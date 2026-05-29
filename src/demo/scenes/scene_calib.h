/**
 * @file    scene_calib.h
 * @brief   Scene: XPT2046 touch screen calibration helper
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Displays live raw ADC values and accumulated min/max as the user taps
 *  the four screen corners. A live crosshair shows where the current
 *  calibration maps each touch. Note the four values and copy them to
 *  ui_nav.c: TOUCH_X_MIN, TOUCH_X_MAX, TOUCH_Y_MIN, TOUCH_Y_MAX.
 */

#ifndef _SCENE_CALIB_H_
#define _SCENE_CALIB_H_

#include <stdint.h>

void SceneCalib_OnEnter(void);
void SceneCalib_OnUpdate(uint32_t now_ms);
void SceneCalib_OnExit(void);

#endif
