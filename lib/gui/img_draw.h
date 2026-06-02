/**
 * @file    img_draw.h
 * @brief   LCD image display from W25Q64 (raw RGB565) with cross fallback
 * @version 1.0
 * @date    Created:       2026-05-30
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#ifndef _IMG_DRAW_H_
#define _IMG_DRAW_H_

#include "res_map.h"
#include <stdint.h>

/**
 * @brief  Render a menu image from W25Q64 by name.
 *
 * Scans all installed image slots for a matching name (case-insensitive).
 * Fallback chain:
 *   1. Slot found by name           → display it
 *   2. "undef_menu" slot found      → display generic placeholder
 *   3. Neither found                → ImgDraw_Cross (dark cross)
 *
 * @param[in] name  Image name (e.g. "keyboard", "joystick") — case-insensitive.
 * @param[in] x     Top-left X on screen.
 * @param[in] y     Top-left Y on screen.
 */
void ImgDraw_ByName(const char *name, int16_t x, int16_t y);

/**
 * @brief  Render a menu image from W25Q64 by direct slot index.
 *
 * Reads pixels from RES_IMG_PIX_ADDR(slot).  Use this only when the slot
 * index is already known (e.g. the flash image browser in scene_image).
 * For menus, prefer ImgDraw_ByName().
 *
 * @param[in] slot  Slot index (0 … RES_IMG_MAX_SLOTS-1).
 * @param[in] x     Top-left X on screen.
 * @param[in] y     Top-left Y on screen.
 */
void ImgDraw_FromFlash(uint8_t slot, int16_t x, int16_t y);

/**
 * @brief  Draw a diagonal cross (✕) inside a rectangle — fallback for missing images.
 *
 * Fills the rectangle with @p bg_color then draws two grey diagonal lines.
 *
 * @param[in] x         Top-left X.
 * @param[in] y         Top-left Y.
 * @param[in] bg_color  Fill color for the rectangle.
 */
void ImgDraw_Cross(int16_t x, int16_t y, uint16_t bg_color);

#endif
