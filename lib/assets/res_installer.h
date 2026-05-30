/**
 * @file    res_installer.h
 * @brief   One-shot resource installer: SD /res/ → W25Q64
 * @version 1.0
 * @date    Created:       2026-05-30
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Runs once at startup.  If the magic word at RES_MAGIC_ADDR is absent,
 *  the installer mounts the SD card, looks for image files under /res/pic/,
 *  decodes them (PNG via pngle; raw RGB565 .bin as fallback), writes them
 *  to W25Q64, and stamps the magic word when done.
 *
 *  If the SD card is absent or /res/pic/ is missing, the function returns
 *  immediately.  Subsequent boots with the magic present also skip the
 *  installer, so image display still works from cached flash data.
 *
 *  To force reinstall: erase the magic sector with an external tool, or
 *  reflash the W25Q64 region starting at RES_BASE_ADDR.
 */

#ifndef _RES_INSTALLER_H_
#define _RES_INSTALLER_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  Run the resource installer.
 *
 * Checks whether resources are already installed (magic word present in
 * W25Q64).  If not, attempts to copy images from SD card to W25Q64.
 * Safe to call unconditionally at application startup.
 */
void ResInstaller_Run(void);

/**
 * @brief  Return true if installation has been completed at least once.
 *
 * A true result only means the installer ran to completion; individual
 * slots may still be invalid (file not found).  Use ResInstaller_IsSlotValid()
 * to check a specific image.
 */
bool ResInstaller_IsInstalled(void);

/**
 * @brief  Return true if image slot @p slot was successfully written.
 *
 * @param[in] slot  Slot index (0 … RES_IMG_SLOT_COUNT-1).
 */
bool ResInstaller_IsSlotValid(uint8_t slot);

#endif
