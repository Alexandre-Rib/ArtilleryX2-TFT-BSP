#define _GNU_SOURCE

#include "ui_draw.h"
#include <stdio.h>
#include <malloc.h>
#include "ff.h"
#include "flash_map.h"
#include "LCD_Init.h"
#include "w25qxx.h"
#include "LCD_Colors.h"
#include "my_misc.h"

#define dbg_printf(...)

#define COLOR_BYTE_SIZE sizeof(uint16_t)  // RGB565 color byte is equal to uint16_t

#ifdef STM32_HAS_FSMC

// defined and implemented on lcd_dma.h / .c
void lcd_frame_display(uint16_t sx, uint16_t sy, uint16_t w, uint16_t h, uint32_t addr);

#else

void lcd_frame_display(uint16_t sx, uint16_t sy, uint16_t w, uint16_t h, uint32_t addr)
{
  uint16_t x, y;
  uint16_t color = 0;

  LCD_SetWindow(sx, sy, sx + w - 1, sy + h - 1);

  W25Qxx_SPI_CS_Set(0);
  W25Qxx_SPI_Read_Write_Byte(CMD_READ_DATA);
  W25Qxx_SPI_Read_Write_Byte((addr & 0xFF0000) >> 16);
  W25Qxx_SPI_Read_Write_Byte((addr & 0xFF00) >> 8);
  W25Qxx_SPI_Read_Write_Byte(addr & 0xFF);

  for (y = sy; y < sy + h; y++)
  {
    for (x = sx; x < sx + w; x++)
    {
      color = (W25Qxx_SPI_Read_Write_Byte(W25QXX_DUMMY_BYTE) << 8);
      color |= W25Qxx_SPI_Read_Write_Byte(W25QXX_DUMMY_BYTE);
      LCD_WR_16BITS_DATA(color);
    }
  }

  W25Qxx_SPI_CS_Set(1);
}

#endif

void lcd_buffer_display(uint16_t sx, uint16_t sy, uint16_t w, uint16_t h, uint16_t * buf, GUI_RECT * limit)
{
  uint16_t wl = w - limit->x1;
  uint16_t hl = h - limit->y1;
  uint16_t x;
  uint16_t y;

  LCD_SetWindow(sx + limit->x0, sy + limit->y0, sx + wl - 1, sy + hl - 1);

  for (y = limit->y0; y < hl; y++)
  {
    for (x = limit->x0; x < wl; x++)
    {
      LCD_WR_16BITS_DATA(buf[(y * w) + x]);
    }
  }
}

void getBMPsize(BMP_INFO * bmp)
{
  if (!bmp->address && bmp->index < ICON_NULL)
    bmp->address = ICON_ADDR(bmp->index);

  FlashMap_Read(bmp->address, (uint8_t *) &bmp->width, COLOR_BYTE_SIZE);
  bmp->address += COLOR_BYTE_SIZE;
  FlashMap_Read(bmp->address, (uint8_t *) &bmp->height, COLOR_BYTE_SIZE);
  bmp->address += COLOR_BYTE_SIZE;
}

static inline void bmpToBuffer(uint16_t * buf, GUI_POINT startPoint, GUI_POINT endPoint, BMP_INFO * iconInfo)
{
  uint16_t frameLines = (endPoint.y - startPoint.y);  // total lines in frame
  uint16_t blockLines = (endPoint.y >= iconInfo->height) ? (iconInfo->height - startPoint.y) : frameLines;  // total drawable lines

  uint16_t frameWidth = (endPoint.x - startPoint.x);  // total frame width
  uint16_t blockWidth = (endPoint.x >= iconInfo->width) ? (iconInfo->width - startPoint.x) : frameWidth;  // total drawable width
  uint16_t bgWidth = frameWidth - blockWidth;  // total empty width to be filled with bg color
  uint16_t color = 0;

  // move address to block starting point
  iconInfo->address += ((iconInfo->width * startPoint.y) + startPoint.x) * COLOR_BYTE_SIZE;

  for (uint16_t y = 0; y < blockLines; y++)
  {
    W25Qxx_SPI_CS_Set(0);
    W25Qxx_SPI_Read_Write_Byte(CMD_READ_DATA);
    W25Qxx_SPI_Read_Write_Byte((iconInfo->address & 0xFF0000) >> 16);
    W25Qxx_SPI_Read_Write_Byte((iconInfo->address & 0xFF00) >> 8);
    W25Qxx_SPI_Read_Write_Byte(iconInfo->address & 0xFF);

    for (uint8_t x = 0; x < blockWidth; x++)
    {
      color = (W25Qxx_SPI_Read_Write_Byte(W25QXX_DUMMY_BYTE) << 8);
      color |= W25Qxx_SPI_Read_Write_Byte(W25QXX_DUMMY_BYTE);
      buf[(y * frameWidth) + x] = color;
    }

    W25Qxx_SPI_CS_Set(1);

    if (bgWidth)
    {
      for (uint8_t x = blockWidth; x < frameWidth; x++)
      {
        buf[(y * frameWidth) + x] = BLACK;
      }
    }

    iconInfo->address += iconInfo->width * COLOR_BYTE_SIZE;
  }

  // fill empty frame lines with background color
  for (uint16_t i = (blockLines * frameWidth); i < (frameLines * frameWidth); i++)
  {
    buf[i] = BLACK;
  }
}

// draw an image from specific address on flash (sx & sy cordinates for top left of image, w width, h height, addr flash byte address)
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

// load the selected area of bmp icon from flash to buffer
void ICON_ReadBuffer(uint16_t * buf, uint16_t x, uint16_t y, int16_t w, int16_t h, uint16_t icon)
{
  BMP_INFO iconInfo = {.index = icon, .address = 0};
  GUI_POINT startPoint = {.x = x, .y = y};
  GUI_POINT endPoint = {.x = x + w, .y = y + h};

  getBMPsize(&iconInfo);
  bmpToBuffer(buf, startPoint, endPoint, &iconInfo);
}

uint16_t ICON_ReadPixel(uint32_t address, uint16_t w, uint16_t h, int16_t x, int16_t y)
{
  // out of range calls
  if (x > w || y > h)
    return BLACK;

  address += ((w * y) + x) * COLOR_BYTE_SIZE;

  W25Qxx_SPI_CS_Set(0);
  W25Qxx_SPI_Read_Write_Byte(CMD_READ_DATA);
  W25Qxx_SPI_Read_Write_Byte((address & 0xFF0000) >> 16);
  W25Qxx_SPI_Read_Write_Byte((address & 0xFF00) >> 8);
  W25Qxx_SPI_Read_Write_Byte(address & 0xFF);

  uint16_t color;

  color = (W25Qxx_SPI_Read_Write_Byte(W25QXX_DUMMY_BYTE) << 8);
  color |= W25Qxx_SPI_Read_Write_Byte(W25QXX_DUMMY_BYTE);

  W25Qxx_SPI_CS_Set(1);

  return color;
}

void SMALLICON_ReadDisplay(uint16_t sx, uint16_t sy, uint8_t icon)
{
  lcd_frame_display(sx, sy, SMALLICON_WIDTH, SMALLICON_HEIGHT, SMALL_ICON_ADDR(icon));
}

void ICON_PressedDisplay(uint16_t sx, uint16_t sy, uint8_t icon)
{
  uint16_t mode = 0x0FF0;
  uint16_t x, y;
  uint16_t color = 0;
  BMP_INFO bmpInfo = {.index = icon, .address = 0};

  getBMPsize(&bmpInfo);

  LCD_SetWindow(sx, sy, sx + bmpInfo.width - 1, sy + bmpInfo.height - 1);

  W25Qxx_SPI_CS_Set(0);
  W25Qxx_SPI_Read_Write_Byte(CMD_READ_DATA);
  W25Qxx_SPI_Read_Write_Byte((bmpInfo.address & 0xFF0000) >> 16);
  W25Qxx_SPI_Read_Write_Byte((bmpInfo.address & 0xFF00) >> 8);
  W25Qxx_SPI_Read_Write_Byte(bmpInfo.address & 0xFF);

  for (y = sy; y < sy + bmpInfo.width; y++)
  {
    for (x = sx; x < sx + bmpInfo.height; x++)
    {
      color  = (W25Qxx_SPI_Read_Write_Byte(W25QXX_DUMMY_BYTE) << 8);
      color |= W25Qxx_SPI_Read_Write_Byte(W25QXX_DUMMY_BYTE);
      LCD_WR_16BITS_DATA(color & mode);
    }
  }

  W25Qxx_SPI_CS_Set(1);
}
