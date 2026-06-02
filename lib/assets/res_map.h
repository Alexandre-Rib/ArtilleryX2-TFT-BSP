/**
 * @file    res_map.h
 * @brief   BSP resource layout in external W25Q64 flash
 * @version 3.0
 * @note    BSP3 — image slots auto-découverts, pas d'indices hardcodés
 *
 *  Layout (base = 7 MB) :
 *    [0x700000] Magic sector (4 KB) — tampon 4 octets "BSP3"
 *    [0x701000] Image slot 0 — 16 KB  [header 16 o + pixels RGB565]
 *    [0x705000] Image slot 1 — 16 KB
 *    ...
 *    [0x740000] Image slot 15 — 16 KB  (RES_IMG_MAX_SLOTS = 16)
 *    [0x741000] Sound slot 0 — 4 KB    (inchangé)
 *    ...
 *    [0x7FF000] Settings sector (réservé)
 *
 *  Format d'un slot image :
 *    [0..15]  char name[16]   — nom de fichier sans extension, majuscules, null-padded
 *    [16..]   RGB565 pixels   — RES_IMG_W × RES_IMG_H × 2 octets (12 800)
 *
 *  Les slots sont remplis dynamiquement par l'installateur qui scanne /res/pic/.bmp.
 *  Accès par nom via ImgDraw_ByName() — aucun indice hardcodé dans le code.
 *
 *  Chaîne de fallback (ImgDraw_ByName) :
 *    1. Nom demandé trouvé     → affichage
 *    2. "undef_menu" trouvé   → affichage de l'image générique
 *    3. Ni l'un ni l'autre    → croix sombre
 */

#ifndef _RES_MAP_H_
#define _RES_MAP_H_

#include <stdint.h>

// ---------------------------------------------------------------------------
// Magic
// ---------------------------------------------------------------------------
#define RES_BASE_ADDR       0x700000u
#define RES_MAGIC_ADDR      RES_BASE_ADDR
#define RES_MAGIC_VALUE     0x42535033u   // "BSP3"

// ---------------------------------------------------------------------------
// Image slots
// ---------------------------------------------------------------------------
#define RES_IMG_W           80u
#define RES_IMG_H           80u
#define RES_IMG_BYTES       ((uint32_t)RES_IMG_W * RES_IMG_H * 2u)   // 12 800

#define RES_IMG_NAME_LEN    16u           // octets — nom uppercase, null-padded
#define RES_IMG_HDR_SIZE    RES_IMG_NAME_LEN
#define RES_IMG_SLOT_SIZE   0x4000u       // 16 KB par slot (header + pixels + padding)
#define RES_IMG_MAX_SLOTS   16u           // capacité flash (16 × 16 KB = 256 KB)
#define RES_IMG_BASE_ADDR   (RES_BASE_ADDR + 0x1000u)
#define RES_IMG_ADDR(n)     (RES_IMG_BASE_ADDR + (uint32_t)(n) * RES_IMG_SLOT_SIZE)
#define RES_IMG_PIX_ADDR(n) (RES_IMG_ADDR(n) + (uint32_t)RES_IMG_HDR_SIZE)

// Nom de l'image de fallback générique (undef_menu.bmp)
#define RES_IMG_NAME_UNDEF  "undef_menu"

// ---------------------------------------------------------------------------
// Sound slots (layout inchangé, base décalée selon RES_IMG_MAX_SLOTS)
// ---------------------------------------------------------------------------
#define RES_SND_SLOT_SIZE   0x1000u
#define RES_SND_SLOT_COUNT  16u
#define RES_SND_BASE_ADDR   (RES_IMG_BASE_ADDR + (uint32_t)RES_IMG_MAX_SLOTS * RES_IMG_SLOT_SIZE)
#define RES_SND_ADDR(n)     (RES_SND_BASE_ADDR + (uint32_t)(n) * RES_SND_SLOT_SIZE)

#endif /* _RES_MAP_H_ */
