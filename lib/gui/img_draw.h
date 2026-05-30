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
 * @brief  Render a menu image from W25Q64 to the LCD.
 *
 * Reads RES_IMG_W × RES_IMG_H pixels stored as raw RGB565 big-endian at
 * RES_IMG_ADDR(slot) and pushes them to the display via LCD_SetWindow +
 * LCD_WR_16BITS_DATA.
 *
 * If the slot is not valid (ResInstaller_IsSlotValid() returns false),
 * ImgDraw_Cross() is drawn instead.
 *
 * @param[in] slot  Image slot index (RES_IMG_PICTURE … RES_IMG_UNDEF).
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
