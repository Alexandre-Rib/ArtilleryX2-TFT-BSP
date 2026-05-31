/**
 * @file    res_map.h
 * @brief   BSP resource layout in external W25Q64 flash
 * @version 2.0
 * @date    Created:       2026-05-30
 *          Last modified: 2026-05-31
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Layout (starting at RES_BASE_ADDR = 7 MB):
 *    [0x700000] Magic sector (4 KB) -- installation stamp + per-slot validity flags
 *    [0x701000] Image slot 0 -- 16 KB
 *    [0x705000] Image slot 1 -- 16 KB
 *    [0x709000] Image slot 2 -- 16 KB
 *    [0x70D000] Image slot 3 -- 16 KB
 *    [0x711000] Image slot 4 -- 16 KB
 *    [0x715000] Image slot 5 -- 16 KB
 *    [0x719000] Sound slot 0 -- 4 KB  (first .snd installed)
 *    [0x71A000] Sound slot 1 -- 4 KB
 *    ...up to slot 15...
 *    [0x728000] Sound slot 15 -- 4 KB  (last possible slot)
 *    ...
 *    [0x7FF000] Settings sector (reserved)
 *
 *  Sound slot layout:
 *    [0..7]   char name[8]       -- uppercase name, null-padded (e.g. "TWINKLE\0")
 *    [8..N]   ToneNote_t notes[] -- {uint16_t freq; uint16_t dur_ms;} pairs
 *    [N+0..3] {0xFF,0xFF,0xFF,0xFF} -- end marker
 *    Uninstalled slot: all 0xFF bytes; first byte 0xFF means "empty".
 *
 *  The installer fills sound slots dynamically from all *.snd files found in
 *  res/sound/ on the SD card -- no hardcoded file list in firmware.
 *  scene_sound discovers installed slots at runtime by scanning headers.
 */

#ifndef _RES_MAP_H_
#define _RES_MAP_H_

#include <stdint.h>

// ---------------------------------------------------------------------------
// Magic -- written last during installation so a power-fail leaves magic absent
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
// Image slot layout
// ---------------------------------------------------------------------------
#define RES_IMG_SLOT_SIZE   0x4000u   // 16 KB per slot (4 sectors x 4 KB)
#define RES_IMG_SLOT_COUNT  6u
#define RES_IMG_BASE_ADDR   (RES_BASE_ADDR + 0x1000u)
#define RES_IMG_ADDR(n)     (RES_IMG_BASE_ADDR + (uint32_t)(n) * RES_IMG_SLOT_SIZE)

// ---------------------------------------------------------------------------
// Image slot indices
// ---------------------------------------------------------------------------
#define RES_IMG_PICTURE     0u   // /res/pic/picture.bmp
#define RES_IMG_ANIMATION   1u   // /res/pic/animation.bmp
#define RES_IMG_SOUND       2u   // /res/pic/sound.bmp
#define RES_IMG_CALIBRATION 3u   // /res/pic/calibration.bmp
#define RES_IMG_UNDEF       4u   // /res/pic/undef_menu.bmp
#define RES_IMG_KEYBOARD    5u   // /res/pic/keyboard.bmp

// ---------------------------------------------------------------------------
// Sound slot layout
// ---------------------------------------------------------------------------
// RES_SND_SLOT_COUNT is a compile-time upper bound.
// Actual installed count is discovered at runtime by scanning headers.
// Adding a new melody = drop a .snd in res/sound/ and reinstall. No code change.
// ---------------------------------------------------------------------------
#define RES_SND_SLOT_SIZE   0x1000u   // 4 KB = one W25Q64 sector (~1022 notes max)
#define RES_SND_SLOT_COUNT  16u
#define RES_SND_BASE_ADDR   (RES_IMG_BASE_ADDR + (uint32_t)RES_IMG_SLOT_COUNT * RES_IMG_SLOT_SIZE)
#define RES_SND_ADDR(n)     (RES_SND_BASE_ADDR + (uint32_t)(n) * RES_SND_SLOT_SIZE)

#endif
