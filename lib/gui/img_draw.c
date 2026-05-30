/**
 * @file    img_draw.c
 * @brief   LCD image display from W25Q64 with two-level fallback
 * @version 2.0
 * @date    Created:       2026-05-30
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Fallback chain (transparent to callers):
 *    1. Try primary slot → if valid, render and return
 *    2. Try RES_IMG_UNDEF  → if valid, render and return
 *    3. Draw a red cross   → always visible
 */

#include "img_draw.h"
#include "res_installer.h"
#include "flash_map.h"
#include "LCD_Init.h"
#include "GUI.h"

#define CHUNK_BYTES  256u
static uint8_t s_chunk[CHUNK_BYTES];

// Draw raw RGB565 (big-endian) from W25Q64 to the LCD at (x, y).
static void draw_slot(uint32_t addr, int16_t x, int16_t y)
{
    uint32_t total = RES_IMG_BYTES;

    LCD_SetWindow((uint16_t)x, (uint16_t)y,
                  (uint16_t)(x + (int16_t)RES_IMG_W - 1),
                  (uint16_t)(y + (int16_t)RES_IMG_H - 1));

    for (uint32_t off = 0; off < total; off += CHUNK_BYTES) {
        uint32_t n = (total - off < CHUNK_BYTES) ? (total - off) : CHUNK_BYTES;
        FlashMap_Read(addr + off, s_chunk, n);
        for (uint32_t i = 0; i < n; i += 2u) {
            uint16_t px = ((uint16_t)s_chunk[i] << 8) | s_chunk[i + 1u];
            LCD_WR_16BITS_DATA(px);
        }
    }
}

void ImgDraw_FromFlash(uint8_t slot, int16_t x, int16_t y)
{
    // 1. Primary slot
    if (ResInstaller_IsSlotValid(slot)) {
        draw_slot(RES_IMG_ADDR(slot), x, y);
        return;
    }

    // 2. Fallback: undef_menu.bmp (don't recurse if primary IS undef)
    if (slot != RES_IMG_UNDEF && ResInstaller_IsSlotValid(RES_IMG_UNDEF)) {
        draw_slot(RES_IMG_ADDR(RES_IMG_UNDEF), x, y);
        return;
    }

    // 3. Last resort: visible red cross
    ImgDraw_Cross(x, y, 0x2104u);
}

void ImgDraw_Cross(int16_t x, int16_t y, uint16_t bg_color)
{
    GUI_FillRectColor((uint16_t)x, (uint16_t)y,
                      (uint16_t)(x + (int16_t)RES_IMG_W),
                      (uint16_t)(y + (int16_t)RES_IMG_H),
                      bg_color);

    GUI_SetColor(0xF800u);  // bright red — visible on any dark background
    GUI_DrawLine((uint16_t)(x + 8),
                 (uint16_t)(y + 8),
                 (uint16_t)(x + (int16_t)RES_IMG_W - 9),
                 (uint16_t)(y + (int16_t)RES_IMG_H - 9));
    GUI_DrawLine((uint16_t)(x + (int16_t)RES_IMG_W - 9),
                 (uint16_t)(y + 8),
                 (uint16_t)(x + 8),
                 (uint16_t)(y + (int16_t)RES_IMG_H - 9));
}
