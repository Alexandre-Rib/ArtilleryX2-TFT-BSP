/**
 * @file    demo_app.h
 * @brief   BSP showcase demo — main menu + scene dispatcher
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#ifndef _DEMO_APP_H_
#define _DEMO_APP_H_

/**
 * @brief  Start the BSP showcase demo application.
 *
 * Initialises navigation input (keyboard + touch) and the USB keyboard driver,
 * loads touch calibration from flash, draws the main menu, then enters the
 * main loop.  Does not return.
 */
void DemoApp_Run(void);

/**
 * @brief  Request the active scene to exit back to the main menu.
 *
 * Sets an internal flag that the main loop checks at the end of each iteration.
 * Safe to call from any scene's on_update() function.
 */
void DemoApp_RequestExit(void);

#endif
