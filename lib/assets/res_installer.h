/**
 * @file    res_installer.h
 * @brief   One-shot resource installer: SD /res/ → W25Q64
 * @version 2.1
 * @date    Created:       2026-05-30
 *          Last modified: 2026-05-31
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Trigger: presence of a "res/" directory at the root of the SD card.
 *  ResInstaller_Run() copies BMP files from SD /res/pic/ to W25Q64, then
 *  renames the SD directory to "res_cur/" so the installer skips on the
 *  next boot.
 *
 *  To reinstall: put a fresh "res/" directory on the SD (remove or rename
 *  the existing "res_cur/" first if needed).
 *
 *  Typical startup sequence in demo_app:
 *    bool installed = ResInstaller_Run();
 *    if (installed) {
 *        // optionally run calibration here if no settings saved
 *        ResInstaller_ShowResult();   // blocks until screen press
 *    }
 */

#ifndef _RES_INSTALLER_H_
#define _RES_INSTALLER_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  Run the resource installer.
 *
 * Mounts the SD card and checks for a "res/" directory at the root.
 * If found, copies images from /res/pic/ to W25Q64 and renames the
 * directory to "res_cur/".  Safe to call unconditionally at startup.
 *
 * @return true   An install was performed this boot.
 * @return false  Nothing to install (no "res/" on SD, or SD absent).
 */
bool ResInstaller_Run(void);

/**
 * @brief  Show the install result screen and block until the screen is pressed.
 *
 * Reads slot status codes from the W25Q64 magic sector and displays only
 * slots that are not OK (WAR_DIM, MISS, ERR_*).  If all slots are OK the
 * function returns immediately.  Call after ResInstaller_Run() returned true
 * and after touch calibration has been set up.
 */
void ResInstaller_ShowResult(void);

/**
 * @brief  Return true if installation has been completed at least once.
 *
 * A true result means the magic word is present in W25Q64; individual
 * slots may still be invalid.  Use ResInstaller_IsSlotValid() to check
 * a specific image.
 */
bool ResInstaller_IsInstalled(void);

/**
 * @brief  Return true if image slot @p slot was successfully written.
 *
 * @param[in] slot  Slot index (0 … RES_IMG_SLOT_COUNT-1).
 */
bool ResInstaller_IsSlotValid(uint8_t slot);

#endif
