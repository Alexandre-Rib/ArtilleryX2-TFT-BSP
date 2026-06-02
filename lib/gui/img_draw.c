/**
 * @file    img_draw.c
 * @brief   LCD image display depuis W25Q64 — accès par nom ou par indice de slot
 * @version 3.0  (BSP3: pixels à RES_IMG_PIX_ADDR, lookup par nom)
 *
 *  Chaîne de fallback (ImgDraw_ByName) :
 *    1. Nom demandé trouvé dans les slots → affichage
 *    2. Slot "undef_menu" trouvé         → affichage de l'image générique
 *    3. Ni l'un ni l'autre               → croix sombre (ImgDraw_Cross)
 */

#include "img_draw.h"
#include "res_installer.h"
#include "flash_map.h"
#include "LCD_Init.h"
#include "GUI.h"
#include <string.h>

#define CHUNK_BYTES  256u
static uint8_t s_chunk[CHUNK_BYTES];

// ---------------------------------------------------------------------------
// Rendu brut : lit les pixels RGB565 (big-endian) depuis une adresse flash
// ---------------------------------------------------------------------------
static void draw_pixels(uint32_t addr, int16_t x, int16_t y)
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

// ---------------------------------------------------------------------------
// Comparaison nom slot (16 octets, majuscules, null-padded) avec query
// ---------------------------------------------------------------------------
static bool name_eq(const uint8_t stored[RES_IMG_NAME_LEN], const char *query)
{
    uint8_t upper[RES_IMG_NAME_LEN];
    memset(upper, 0, RES_IMG_NAME_LEN);
    for (uint8_t i = 0; i < (uint8_t)RES_IMG_NAME_LEN && query[i]; i++) {
        char c = query[i];
        upper[i] = (uint8_t)((c >= 'a' && c <= 'z') ? (char)(c - 32) : c);
    }
    return memcmp(stored, upper, RES_IMG_NAME_LEN) == 0;
}

// Retourne l'indice du slot dont le header correspond au nom, ou -1 si absent
static int8_t find_slot(const char *name)
{
    uint8_t hdr[RES_IMG_NAME_LEN];
    for (uint8_t i = 0; i < (uint8_t)RES_IMG_MAX_SLOTS; i++) {
        FlashMap_Read(RES_IMG_ADDR(i), hdr, RES_IMG_NAME_LEN);
        if (hdr[0] == 0xFFu) continue;
        if (name_eq(hdr, name)) return (int8_t)i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// API publique
// ---------------------------------------------------------------------------

void ImgDraw_ByName(const char *name, int16_t x, int16_t y)
{
    // 1. Slot correspondant au nom demandé
    int8_t slot = find_slot(name);
    if (slot >= 0) {
        draw_pixels(RES_IMG_PIX_ADDR((uint8_t)slot), x, y);
        return;
    }

    // 2. Fallback : image générique "undef_menu" (sauf si c'est déjà elle qu'on cherche)
    uint8_t upper_query[RES_IMG_NAME_LEN];
    memset(upper_query, 0, RES_IMG_NAME_LEN);
    for (uint8_t i = 0; i < (uint8_t)RES_IMG_NAME_LEN && RES_IMG_NAME_UNDEF[i]; i++) {
        char c = RES_IMG_NAME_UNDEF[i];
        upper_query[i] = (uint8_t)((c >= 'a' && c <= 'z') ? (char)(c - 32) : c);
    }
    if (!name_eq(upper_query, name)) {   // évite la boucle infinie
        int8_t undef_slot = find_slot(RES_IMG_NAME_UNDEF);
        if (undef_slot >= 0) {
            draw_pixels(RES_IMG_PIX_ADDR((uint8_t)undef_slot), x, y);
            return;
        }
    }

    // 3. Dernier recours : croix sombre
    ImgDraw_Cross(x, y, 0x2104u);
}

void ImgDraw_FromFlash(uint8_t slot, int16_t x, int16_t y)
{
    if (slot < (uint8_t)RES_IMG_MAX_SLOTS)
        draw_pixels(RES_IMG_PIX_ADDR(slot), x, y);
    else
        ImgDraw_Cross(x, y, 0x2104u);
}

void ImgDraw_Cross(int16_t x, int16_t y, uint16_t bg_color)
{
    GUI_FillRectColor((uint16_t)x, (uint16_t)y,
                      (uint16_t)(x + (int16_t)RES_IMG_W),
                      (uint16_t)(y + (int16_t)RES_IMG_H),
                      bg_color);

    GUI_SetColor(0x4228u);   // gris sombre — "croix noire"
    GUI_DrawLine((uint16_t)(x + 8),
                 (uint16_t)(y + 8),
                 (uint16_t)(x + (int16_t)RES_IMG_W - 9),
                 (uint16_t)(y + (int16_t)RES_IMG_H - 9));
    GUI_DrawLine((uint16_t)(x + (int16_t)RES_IMG_W - 9),
                 (uint16_t)(y + 8),
                 (uint16_t)(x + 8),
                 (uint16_t)(y + (int16_t)RES_IMG_H - 9));
}
