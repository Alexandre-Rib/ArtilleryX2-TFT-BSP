#ifndef _FONT_ATLAS_H_
#define _FONT_ATLAS_H_

#include <stdint.h>
#include "utf8.h"   // UTF8_CHAR

// One entry in the font table: describes a Unicode range stored in W25Q64
typedef struct
{
  uint32_t startCodePoint;       // first Unicode code point in this range
  uint32_t endCodePoint;         // last Unicode code point in this range
  uint8_t  pixelHeight;          // rendered glyph height (pixels)
  uint8_t  pixelWidth;           // rendered glyph width  (pixels)
  uint32_t bitMapStartAddr;      // start address of this font in W25Q64
  uint8_t  bitMapHeight;         // bitmap cell height
  uint8_t  bitMapWidth;          // bitmap cell width
  uint32_t bitMapStartCodePoint; // first code point stored at bitMapStartAddr
} FONT_BITMAP;

// Result of resolving one character: UTF-8 info + where its glyph lives in flash
typedef struct
{
  uint8_t  bytes;        // UTF-8 bytes consumed
  uint32_t codePoint;    // Unicode code point
  uint8_t  pixelHeight;  // glyph height to render (pixels)
  uint8_t  pixelWidth;   // glyph width  to render (pixels)
  uint32_t bitMapAddr;   // address of this glyph's bitmap in W25Q64 (0 = not found)
} CHAR_INFO;

// Select normal or large font (pass FONT_SIZE_NORMAL or FONT_SIZE_LARGE)
void FontAtlas_SetSize(uint16_t size);

// Decode one UTF-8 character and resolve its flash address + pixel dimensions
void FontAtlas_GetCharInfo(const uint8_t * ch, CHAR_INFO * pInfo);

#endif