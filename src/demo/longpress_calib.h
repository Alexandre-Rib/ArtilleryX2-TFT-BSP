/**
 * @file  longpress_calib.h
 * @brief Long-press touch trigger for calibration — overlay drawn from any screen.
 *
 *  Hold the touchscreen for 30 s to trigger calibration:
 *    0–25 s  : silent (normal operation)
 *   25–30 s  : countdown overlay 5→4→3→2→1 drawn on top of the current screen
 *   ≥30 s    : "release" message shown
 *   on release: 2-second countdown, then LPC_LAUNCH is returned
 *
 *  The caller is responsible for:
 *    - Calling LongpressCalib_Update() every main-loop iteration
 *    - Skipping scene updates while LongpressCalib_IsBlocking() is true
 *    - Launching SceneCalib on LPC_LAUNCH
 *    - Restoring/redrawing the previous screen on LPC_CANCELLED
 *    - Calling LongpressCalib_Reset() after calib returns
 */

#ifndef _LONGPRESS_CALIB_H_
#define _LONGPRESS_CALIB_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    LPC_IDLE,       ///< No action needed.
    LPC_LAUNCH,     ///< 2-second wait elapsed — launch calibration scene now.
    LPC_CANCELLED,  ///< Overlay was visible but user released early — redraw screen.
} LpcStatus_t;

/** Call once at startup. */
void LongpressCalib_Init(void);

/**
 * @brief  Call every main-loop iteration (after MouseCursor_Hide, before Show).
 *
 * Polls XPT2046_Read_Pen(), advances the state machine, draws overlay when needed.
 *
 * @param[in] now_ms  Current timestamp from OS_GetTimeMs().
 * @return LPC_IDLE normally; LPC_LAUNCH when calib should start; LPC_CANCELLED if aborted.
 */
LpcStatus_t LongpressCalib_Update(uint32_t now_ms);

/**
 * @brief  Returns true while the overlay is active (countdown, wait-release, or 2-s wait).
 *
 * Scene updates should be skipped while blocking to prevent the scene from
 * drawing over the overlay.
 */
bool LongpressCalib_IsBlocking(void);

/** Reset state machine after calib returns (or after an aborted overlay). */
void LongpressCalib_Reset(void);

#endif /* _LONGPRESS_CALIB_H_ */
