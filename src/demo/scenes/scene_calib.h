/**
 * @file    scene_calib.h
 * @brief   Scene: XPT2046 touch screen calibration
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Two sub-modes selectable from a footer menu (keyboard + touch):
 *
 *  LIVE mode (default on entry):
 *    Shows raw ADC bars, running MIN/MAX for each axis, and a live crosshair
 *    mapped through the current calibration.  Useful for verifying accuracy
 *    after calibration.
 *
 *  PROCEDURE mode:
 *    Guided 4-corner calibration (TL -> TR -> BR -> BL).  Any tap on the screen
 *    captures the current corner — footer detection is disabled because the
 *    uncalibrated ADC cannot reliably distinguish bottom corners from the footer.
 *    After all four corners are captured the calibration is saved automatically.
 *    QUIT is accessible via ESC key only.
 *
 *  ESC key behaviour:
 *    In PROCEDURE mode — returns to LIVE mode (event consumed, returns true).
 *    In LIVE mode      — not consumed (returns false); demo_app exits the scene.
 */

#ifndef _SCENE_CALIB_H_
#define _SCENE_CALIB_H_

#include "ui_nav.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Enter the calibration scene.
 *
 * Initialises state, resets MIN/MAX accumulators, and draws the LIVE layout.
 */
void SceneCalib_OnEnter(void);

/**
 * @brief  Update the calibration scene for one main-loop iteration.
 *
 * @param[in] now_ms  Current timestamp from OS_GetTimeMs().
 * @param[in] event   Navigation event from Navigation_Poll().
 * @return true   The event was consumed by this scene (e.g. ESC in PROCEDURE mode).
 * @return false  The event was not consumed; demo_app may act on it (e.g. exit scene).
 */
bool SceneCalib_OnUpdate(uint32_t now_ms, NavigationEvent_t event);

/**
 * @brief  Exit the calibration scene.
 *
 * Saves are explicit (SAVE button only) — nothing is persisted here.
 */
void SceneCalib_OnExit(void);

#endif
