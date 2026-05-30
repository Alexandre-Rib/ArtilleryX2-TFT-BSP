/**
 * @file    res_map.h
 * @brief   BSP resource layout in external W25Q64 flash
 * @version 1.0
 * @date    Created:       2026-05-30
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Layout (starting at RES_BASE_ADDR = 7 MB):
 *    [0x700000] Magic sector (4 KB) — installation stamp + per-slot validity flags
 *    [0x701000] Image slot 0 — 16 KB
 *    [0x705000] Image slot 1 — 16 KB
 *    [0x709000] Image slot 2 — 16 KB
 *    [0x70D000] Image slot 3 — 16 KB
 *    [0x711000] Image slot 4 — 16 KB
 *
 *  All addresses are well below the settings sector at 0x7FF000 and the
 *  original Artillery firmware data (ends around 5 MB).
 *
 *  Image format in flash: raw RGB565, big-endian (high byte first), row-major.
 *  Total per image: RES_IMG_W * RES_IMG_H * 2 = 12 800 bytes.
 *
 *  Magic sector layout (first 8 bytes):
 *    [0] uint32_t  installation magic (RES_MAGIC_VALUE when install is complete)
 *    [4] uint8_t   valid[RES_IMG_SLOT_COUNT] — 1 = slot OK, 0 = not found/error
 */

#ifndef _RES_MAP_H_
#define _RES_MAP_H_

#include <stdint.h>

// ---------------------------------------------------------------------------
// Magic — written last during installation so a power-fail leaves magic absent
// ---------------------------------------------------------------------------
#define RES_BASE_ADDR       0x700000u
#define RES_MAGIC_ADDR      RES_BASE_ADDR
#define RES_MAGIC_VALUE     0x42535031u   // "BSP1"


// ---------------------------------------------------------------------------
// Image geometry
// ---------------------------------------------------------------------------
#define RES_IMG_W           80u
#define RES_IMG_H           80u
#define RES_IMG_BYTES       ((uint32_t)RES_IMG_W * RES_IMG_H * 2u)   // 12 800

// ---------------------------------------------------------------------------
// Slot layout
// ---------------------------------------------------------------------------
#define RES_IMG_SLOT_SIZE   0x4000u   // 16 KB per slot (4 sectors × 4 KB)
#define RES_IMG_SLOT_COUNT  6u
#define RES_IMG_BASE_ADDR   (RES_BASE_ADDR + 0x1000u)   // first slot starts after magic sector
#define RES_IMG_ADDR(n)     (RES_IMG_BASE_ADDR + (uint32_t)(n) * RES_IMG_SLOT_SIZE)

// ---------------------------------------------------------------------------
// Slot indices
// ---------------------------------------------------------------------------
#define RES_IMG_PICTURE     0u   // /res/pic/picture.bmp
#define RES_IMG_ANIMATION   1u   // /res/pic/animation.bmp
#define RES_IMG_SOUND       2u   // /res/pic/sound.bmp
#define RES_IMG_CALIBRATION 3u   // /res/pic/calibration.bmp
#define RES_IMG_UNDEF       4u   // /res/pic/undef_menu.bmp  — fallback when primary slot invalid
#define RES_IMG_KEYBOARD    5u   // /res/pic/keyboard.bmp    — no file → tests fallback to undef

#endif
