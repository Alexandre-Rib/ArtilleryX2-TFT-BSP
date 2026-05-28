#include "font_atlas.h"
#include "mks_tft28.h"  // BYTE_HEIGHT, BYTE_WIDTH, FONT_SIZE_NORMAL, _FONT_H, _FONT_W
#include "flash_map.h"  // BYTE_ASCII_ADDR, LARGE_FONT_ADDR, WORD_UNICODE_ADDR
#include "my_misc.h"    // COUNT()

static uint16_t currentSize = FONT_SIZE_NORMAL;

// Flash address map for all supported font ranges on this board
static const FONT_BITMAP fontTable[] = {
  { // ASCII printable (0x20–0x7E) — normal size
    .startCodePoint       = 0x20,
    .endCodePoint         = 0x7E,
    .pixelHeight          = BYTE_HEIGHT,
    .pixelWidth           = BYTE_WIDTH,
    .bitMapStartAddr      = BYTE_ASCII_ADDR,
    .bitMapHeight         = BYTE_HEIGHT,
    .bitMapWidth          = BYTE_WIDTH,
    .bitMapStartCodePoint = 0x20,
  },
  { // ASCII printable (0x20–0x7E) — large size
    .startCodePoint       = 0x20,
    .endCodePoint         = 0x7E,
    .pixelHeight          = LARGE_BYTE_HEIGHT,
    .pixelWidth           = LARGE_BYTE_WIDTH,
    .bitMapStartAddr      = LARGE_FONT_ADDR,
    .bitMapHeight         = LARGE_BYTE_HEIGHT,
    .bitMapWidth          = LARGE_BYTE_WIDTH,
    .bitMapStartCodePoint = 0x20,
  },
  { // Latin-1 Supplement + Extended-A/B (Czech, Hungarian…)
    .startCodePoint       = 0x80,
    .endCodePoint         = 0x24F,
    .pixelHeight          = BYTE_HEIGHT,
    .pixelWidth           = BYTE_WIDTH,
    .bitMapStartAddr      = WORD_UNICODE_ADDR,
    .bitMapHeight         = BYTE_HEIGHT,
    .bitMapWidth          = BYTE_WIDTH * 2,
    .bitMapStartCodePoint = 0x0,
  },
  { // Greek and Coptic
    .startCodePoint       = 0x370,
    .endCodePoint         = 0x3FF,
    .pixelHeight          = BYTE_HEIGHT,
    .pixelWidth           = BYTE_WIDTH,
    .bitMapStartAddr      = WORD_UNICODE_ADDR,
    .bitMapHeight         = BYTE_HEIGHT,
    .bitMapWidth          = BYTE_WIDTH * 2,
    .bitMapStartCodePoint = 0x0,
  },
  { // Cyrillic
    .startCodePoint       = 0x400,
    .endCodePoint         = 0x451,
    .pixelHeight          = BYTE_HEIGHT,
    .pixelWidth           = BYTE_WIDTH,
    .bitMapStartAddr      = WORD_UNICODE_ADDR,
    .bitMapHeight         = BYTE_HEIGHT,
    .bitMapWidth          = BYTE_WIDTH * 2,
    .bitMapStartCodePoint = 0x0,
  },
  { // Armenian
    .startCodePoint       = 0x530,
    .endCodePoint         = 0x58F,
    .pixelHeight          = BYTE_HEIGHT,
    .pixelWidth           = BYTE_WIDTH,
    .bitMapStartAddr      = WORD_UNICODE_ADDR,
    .bitMapHeight         = BYTE_HEIGHT,
    .bitMapWidth          = BYTE_WIDTH * 2,
    .bitMapStartCodePoint = 0x0,
  },
  { // Catch-all (remaining Unicode → unicode font file)
    .startCodePoint       = 0x9,
    .endCodePoint         = 0xFFFF,
    .pixelHeight          = BYTE_HEIGHT,
    .pixelWidth           = BYTE_WIDTH * 2,
    .bitMapStartAddr      = WORD_UNICODE_ADDR,
    .bitMapHeight         = BYTE_HEIGHT,
    .bitMapWidth          = BYTE_WIDTH * 2,
    .bitMapStartCodePoint = 0x0,
  },
};

void FontAtlas_SetSize(uint16_t size)
{
  currentSize = size;
}

void FontAtlas_GetCharInfo(const uint8_t * ch, CHAR_INFO * pInfo)
{
  pInfo->bytes = 0;

  if (ch == NULL || *ch == 0)
    return;

  UTF8_CHAR decoded;
  utf8_decode(ch, &decoded);
  pInfo->bytes     = decoded.bytes;
  pInfo->codePoint = decoded.codePoint;

  // Control characters below 0x09 have no glyph
  if (pInfo->codePoint < 9)
  {
    pInfo->pixelWidth  = 0;
    pInfo->pixelHeight = 0;
    pInfo->bitMapAddr  = 0;
    return;
  }

  for (uint8_t i = 0; i < COUNT(fontTable); i++)
  {
    const FONT_BITMAP * f = &fontTable[i];

    if (pInfo->codePoint < f->startCodePoint || pInfo->codePoint > f->endCodePoint)
      continue;

    // ASCII has two entries (normal/large): match the currently selected size
    if (pInfo->codePoint >= 0x20 && pInfo->codePoint <= 0x7E)
    {
      if (_FONT_H(currentSize) != f->pixelHeight || _FONT_W(currentSize) != f->pixelWidth)
        continue;
    }

    pInfo->pixelWidth  = f->pixelWidth;
    pInfo->pixelHeight = f->pixelHeight;
    pInfo->bitMapAddr  = f->bitMapStartAddr
                       + (pInfo->codePoint - f->bitMapStartCodePoint)
                       * (f->bitMapHeight * f->bitMapWidth / 8);
    return;
  }

  // Not found
  pInfo->pixelWidth  = 0;
  pInfo->pixelHeight = 0;
  pInfo->bitMapAddr  = 0;
}
