#ifndef _FLASH_MAP_H_
#define _FLASH_MAP_H_

#include <stdint.h>

// ---------------------------------------------------------------------------
// Flash sector geometry
// ---------------------------------------------------------------------------
#define W25QXX_SECTOR_SIZE  0x1000u   // 4 KB

// ---------------------------------------------------------------------------
// Section sizes (bytes) in external W25Q64
// ---------------------------------------------------------------------------
#define LOGO_MAX_SIZE           0x4B000u
#define WORD_UNICODE_SIZE      0x480000u
#define BYTE_ASCII_SIZE          0x1000u
#define LARGE_FONT_SIZE          0x3000u
#define _8X16_FONT_SIZE          0x1000u
#define FLASH_SIGN_SIZE          0x1000u
#define LANGUAGE_SIZE           0x16000u
#define STRINGS_STORE_MAX_SIZE   0x1000u
#define PREHEAT_STORE_MAX_SIZE   0x1000u
#define PRINT_GCODES_MAX_SIZE    0x5000u
#define CUSTOM_GCODE_MAX_SIZE    0x5000u
#define ICON_MAX_SIZE            0x5000u
#define INFOBOX_MAX_SIZE         0xB000u
#define SMALL_ICON_MAX_SIZE      0x2000u

// ---------------------------------------------------------------------------
// Address map â€” sequential layout in W25Q64
// ---------------------------------------------------------------------------
#define LOGO_ADDR             0x0u
#define WORD_UNICODE_ADDR     LOGO_MAX_SIZE
#define BYTE_ASCII_ADDR       (WORD_UNICODE_ADDR   + WORD_UNICODE_SIZE)
#define LARGE_FONT_ADDR       (BYTE_ASCII_ADDR     + BYTE_ASCII_SIZE)
#define _8X16_FONT_ADDR       (LARGE_FONT_ADDR     + LARGE_FONT_SIZE)
#define FLASH_SIGN_ADDR       (_8X16_FONT_ADDR     + _8X16_FONT_SIZE)
#define LANGUAGE_ADDR         (FLASH_SIGN_ADDR     + FLASH_SIGN_SIZE)
#define STRINGS_STORE_ADDR    (LANGUAGE_ADDR       + LANGUAGE_SIZE)
#define PREHEAT_STORE_ADDR    (STRINGS_STORE_ADDR  + STRINGS_STORE_MAX_SIZE)
#define PRINT_GCODES_ADDR     (PREHEAT_STORE_ADDR  + PREHEAT_STORE_MAX_SIZE)
#define CUSTOM_GCODE_ADDR     (PRINT_GCODES_ADDR   + PRINT_GCODES_MAX_SIZE)

#define ICON_ADDR(num)        ((num) * ICON_MAX_SIZE + CUSTOM_GCODE_ADDR + CUSTOM_GCODE_MAX_SIZE)
#define INFOBOX_ADDR          (ICON_ADDR(ICON_PREVIEW) + ICON_MAX_SIZE)
#define SMALL_ICON_START_ADDR (INFOBOX_ADDR + INFOBOX_MAX_SIZE)
#define SMALL_ICON_ADDR(num)  ((num) * SMALL_ICON_MAX_SIZE + SMALL_ICON_START_ADDR)

// ---------------------------------------------------------------------------
// Icon index enum (auto-generated â€” add new icons in icon_list.inc only)
// ---------------------------------------------------------------------------
enum
{
  #define X_ICON(NAME) ICON_##NAME ,
    #include "icon_list.inc"
  #undef X_ICON

  ICON_PREVIEW,
  ICON_NULL
};

// ---------------------------------------------------------------------------
// Board identification
// ---------------------------------------------------------------------------
#define HARDWARE        "MKS_TFT28"
#define HARDWARE_SHORT  "TFT28"

// ---------------------------------------------------------------------------
// Public API â€” callers never need to know about W25Q64 internals
// ---------------------------------------------------------------------------
void FlashMap_Read(uint32_t addr, uint8_t * buf, uint32_t size);
void FlashMap_Write(uint32_t addr, const uint8_t * buf, uint32_t size);
void FlashMap_WritePage(uint32_t addr, const uint8_t * buf, uint16_t size);
void FlashMap_EraseSector(uint32_t addr);

#endif
