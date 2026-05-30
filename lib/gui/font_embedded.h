/**
 * @file    font_embedded.h
 * @brief   8x8 pixel bitmap font stored in MCU flash — no external flash dependency
 * @version 1.0
 * @date    Created:       2026-05-30
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Covers printable ASCII 0x20 (' ') through 0x7E ('~').
 *  Each character is 8 columns wide × 8 rows tall (MSB = leftmost pixel).
 *
 *  Use Font_DrawString() for text that must not depend on W25Q64 font data
 *  (e.g. status messages, confirmations, debug overlays).
 */

#ifndef _FONT_EMBEDDED_H_
#define _FONT_EMBEDDED_H_

#include <stdint.h>

/** Character cell size in pixels. */
#define FONT_EMBEDDED_W  8
#define FONT_EMBEDDED_H  8

/**
 * @brief  Draw a single ASCII character with optional integer scaling.
 *
 * @param[in] x      Top-left X of the character cell (screen pixels).
 * @param[in] y      Top-left Y of the character cell (screen pixels).
 * @param[in] c      ASCII character (0x20–0x7E; others are silently skipped).
 * @param[in] scale  Pixel magnification factor (1 = native 8×8, 2 = 16×16, …).
 * @param[in] color  RGB565 foreground color.
 */
void Font_DrawChar(int16_t x, int16_t y, char c, uint8_t scale, uint16_t color);

/**
 * @brief  Draw a NUL-terminated ASCII string with optional integer scaling.
 *
 * Characters are drawn left-to-right with no wrapping.  Non-printable chars
 * advance the cursor by one character cell but draw nothing.
 *
 * @param[in] x      Top-left X of the first character (screen pixels).
 * @param[in] y      Top-left Y of the string (screen pixels).
 * @param[in] str    NUL-terminated string to render.
 * @param[in] scale  Pixel magnification factor (1 = native 8×8).
 * @param[in] color  RGB565 foreground color.
 */
void Font_DrawString(int16_t x, int16_t y, const char *str,
                     uint8_t scale, uint16_t color);

/**
 * @brief  Draw a NUL-terminated ASCII string centered inside a rectangle.
 *
 * The string is horizontally and vertically centered within [x0,x1) × [y0,y1).
 * If the string is wider than the rectangle it is drawn left-aligned from x0.
 *
 * @param[in] x0     Left edge of the bounding rectangle (inclusive).
 * @param[in] y0     Top edge of the bounding rectangle (inclusive).
 * @param[in] x1     Right edge of the bounding rectangle (exclusive).
 * @param[in] y1     Bottom edge of the bounding rectangle (exclusive).
 * @param[in] str    NUL-terminated string to render.
 * @param[in] scale  Pixel magnification factor.
 * @param[in] color  RGB565 foreground color.
 */
void Font_DrawStringCentered(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                              const char *str, uint8_t scale, uint16_t color);

#endif
