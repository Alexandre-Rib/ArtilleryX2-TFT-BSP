#define _GNU_SOURCE

#include "ui_draw.h"
#include <stdio.h>
#include <malloc.h>
#include "ff.h"
#include "flash_map.h"
#include "LCD_Init.h"
#include "LCD_Colors.h"
#include "my_misc.h"

#define dbg_printf(...)

#define COLOR_BYTE_SIZE sizeof(uint16_t)

// Line buffer for streaming flash → LCD (avoids pixel-by-pixel SPI transactions)
static uint16_t lineBuf[LCD_WIDTH];

#ifdef STM32_HAS_FSMC

// defined and implemented in lcd_dma.h / .c
void lcd_frame_display(uint16_t sx, uint16_t sy, uint16_t w, uint16_t h, uint32_t addr);

#else

void lcd_frame_display(uint16_t sx, uint16_t sy, uint16_t w, uint16_t h, uint32_t addr)
{
  uint32_t lineBytes = w * COLOR_BYTE_SIZE;

  LCD_SetWindow(sx, sy, sx + w - 1, sy + h - 1);

  for (uint16_t y = 0; y < h; y++)
  {
    FlashMap_Read(addr, (uint8_t *) lineBuf, lineBytes);
    addr += lineBytes;

    for (uint16_t x = 0; x < w; x++)
      LCD_WR_16BITS_DATA(lineBuf[x]);
  }
}

#endif

void lcd_buffer_display(uint16_t sx, uint16_t sy, uint16_t w, uint16_t h, uint16_t * buf, GUI_RECT * limit)
{
  uint16_t wl = w - limit->x1;
  uint16_t hl = h - limit->y1;

  LCD_SetWindow(sx + limit->x0, sy + limit->y0, sx + wl - 1, sy + hl - 1);

  for (uint16_t y = limit->y0; y < hl; y++)
    for (uint16_t x = limit->x0; x < wl; x++)
      LCD_WR_16BITS_DATA(buf[(y * w) + x]);
}

void getBMPsize(BMP_INFO * bmp)
{
  if (!bmp->address && bmp->index < ICON_NULL)
    bmp->address = ICON_ADDR(bmp->index);

  FlashMap_Read(bmp->address, (uint8_t *) &bmp->width,  COLOR_BYTE_SIZE);
  bmp->address += COLOR_BYTE_SIZE;
  FlashMap_Read(bmp->address, (uint8_t *) &bmp->height, COLOR_BYTE_SIZE);
  bmp->address += COLOR_BYTE_SIZE;
}

static inline void bmpToBuffer(uint16_t * buf, GUI_POINT startPoint, GUI_POINT endPoint, BMP_INFO * iconInfo)
{
  uint16_t frameLines = (endPoint.y - startPoint.y);
  uint16_t blockLines = (endPoint.y >= iconInfo->height) ? (iconInfo->height - startPoint.y) : frameLines;
  uint16_t frameWidth = (endPoint.x - startPoint.x);
  uint16_t blockWidth = (endPoint.x >= iconInfo->width)  ? (iconInfo->width  - startPoint.x) : frameWidth;
  uint16_t bgWidth    = frameWidth - blockWidth;

  // Offset to starting pixel (row + column)
  iconInfo->address += ((iconInfo->width * startPoint.y) + startPoint.x) * COLOR_BYTE_SIZE;

  for (uint16_t y = 0; y < blockLines; y++)
  {
    FlashMap_Read(iconInfo->address, (uint8_t *) &buf[y * frameWidth], blockWidth * COLOR_BYTE_SIZE);
    iconInfo->address += iconInfo->width * COLOR_BYTE_SIZE;

    if (bgWidth)
      for (uint8_t x = blockWidth; x < frameWidth; x++)
        buf[(y * frameWidth) + x] = BLACK;
  }

  // Fill remaining lines with background
  for (uint16_t i = blockLines * frameWidth; i < frameLines * frameWidth; i++)
    buf[i] = BLACK;
}

void IMAGE_ReadDisplay(uint16_t sx, uint16_t sy, uint32_t address)
{
  BMP_INFO bmpInfo = {.index = ICON_NULL, .address = address};

  getBMPsize(&bmpInfo);
  lcd_frame_display(sx, sy, bmpInfo.width, bmpInfo.height, bmpInfo.address);
}

void LOGO_ReadDisplay(void)
{
  IMAGE_ReadDisplay(0, 0, LOGO_ADDR);
}

void ICON_ReadDisplay(uint16_t sx, uint16_t sy, uint8_t icon)
{
  IMAGE_ReadDisplay(sx, sy, ICON_ADDR(icon));
}

void ICON_ReadBuffer(uint16_t * buf, uint16_t x, uint16_t y, int16_t w, int16_t h, uint16_t icon)
{
  BMP_INFO iconInfo     = {.index = icon, .address = 0};
  GUI_POINT startPoint  = {.x = x,     .y = y};
  GUI_POINT endPoint    = {.x = x + w, .y = y + h};

  getBMPsize(&iconInfo);
  bmpToBuffer(buf, startPoint, endPoint, &iconInfo);
}

uint16_t ICON_ReadPixel(uint32_t address, uint16_t w, uint16_t h, int16_t x, int16_t y)
{
  if (x > w || y > h)
    return BLACK;

  address += ((w * y) + x) * COLOR_BYTE_SIZE;

  uint16_t color;
  FlashMap_Read(address, (uint8_t *) &color, COLOR_BYTE_SIZE);
  return color;
}

void SMALLICON_ReadDisplay(uint16_t sx, uint16_t sy, uint8_t icon)
{
  lcd_frame_display(sx, sy, SMALLICON_WIDTH, SMALLICON_HEIGHT, SMALL_ICON_ADDR(icon));
}

void ICON_PressedDisplay(uint16_t sx, uint16_t sy, uint8_t icon)
{
  const uint16_t mode = 0x0FF0;
  BMP_INFO bmpInfo = {.index = icon, .address = 0};

  getBMPsize(&bmpInfo);
  LCD_SetWindow(sx, sy, sx + bmpInfo.width - 1, sy + bmpInfo.height - 1);

  uint32_t lineBytes = bmpInfo.width * COLOR_BYTE_SIZE;

  for (uint16_t y = 0; y < bmpInfo.height; y++)
  {
    FlashMap_Read(bmpInfo.address, (uint8_t *) lineBuf, lineBytes);
    bmpInfo.address += lineBytes;

    for (uint16_t x = 0; x < bmpInfo.width; x++)
      LCD_WR_16BITS_DATA(lineBuf[x] & mode);
  }
}
