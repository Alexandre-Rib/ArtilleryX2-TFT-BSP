#ifndef _MKS_TFT28_H_
#define _MKS_TFT28_H_

// MCU registers
#include "stm32f10x.h"
// GPIO pin name aliases (PA0…PF15)
#include "STM32Fxx_Pins.h"

// ---------------------------------------------------------------------------
// LCD driver : HX8558 (seul driver supporté)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Display resolution (320x240 landscape)
// ---------------------------------------------------------------------------
#define LCD_WIDTH  320
#define LCD_HEIGHT 240

#define BYTE_HEIGHT       16
#define BYTE_WIDTH        (BYTE_HEIGHT / 2)
#define LARGE_BYTE_HEIGHT 24
#define LARGE_BYTE_WIDTH  (LARGE_BYTE_HEIGHT / 2)

// UI layout constants (used by gui/ui_draw.h and gui/CharIcon.h)
#define ICON_WIDTH   70
#define ICON_HEIGHT  70
#define TITLE_END_Y  30
#define ICON_START_Y (TITLE_END_Y + 10)

#define LIST_ICON_WIDTH  55
#define LIST_ICON_HEIGHT 50
#define INFOBOX_WIDTH    150
#define INFOBOX_HEIGHT   70
#define SMALLICON_WIDTH  45
#define SMALLICON_HEIGHT 40

// Font size helpers
#define _FONT_SIZE(h, w) ((h << 8) | w)
#define _FONT_H(size)    ((size) >> 8)
#define _FONT_W(size)    ((size) & 0xFF)

#define FONT_SIZE_NORMAL _FONT_SIZE(BYTE_HEIGHT, BYTE_WIDTH)
#define FONT_SIZE_LARGE  _FONT_SIZE(LARGE_BYTE_HEIGHT, LARGE_BYTE_WIDTH)

// ---------------------------------------------------------------------------
// Board identification
// ---------------------------------------------------------------------------
#define HARDWARE_MANUFACTURER "MKS_"
#define HARDWARE_VERSION      "TFT28_V4.0"
#define UPDATE_DIR            "TFT28"

// ---------------------------------------------------------------------------
// Buzzer (passif, PWM — TIM5_CH3 sur PA2)
// ---------------------------------------------------------------------------
#define BUZZER_PIN  PA2

// ---------------------------------------------------------------------------
// Touch screen (XPT2046 — software SPI)
// ---------------------------------------------------------------------------
#define XPT2046_CS   PC9
#define XPT2046_SCK  PC10
#define XPT2046_MISO PC11
#define XPT2046_MOSI PC12
#define XPT2046_TPEN PC5

// ---------------------------------------------------------------------------
// External flash W25Q64 (SPI1)
// ---------------------------------------------------------------------------
#define W25Qxx_SPEED  1
#define W25Qxx_SPI    _SPI1
#define W25Qxx_CS_PIN PB9

// ---------------------------------------------------------------------------
// LCD (HX8558, 16-bit parallel)
// ---------------------------------------------------------------------------
#ifndef LCD_DATA_16BIT
  #define LCD_DATA_16BIT 1
#endif

// ---------------------------------------------------------------------------
// USART ports
// ---------------------------------------------------------------------------
#ifndef SERIAL_PORT
  #define SERIAL_PORT   _USART2
  #define SERIAL_PORT_2 _USART1
  #define SERIAL_PORT_3 _USART3
  #define USART2_TX_PIN PD5
  #define USART2_RX_PIN PD6
  #define USART3_TX_PIN PD8
  #define USART3_RX_PIN PD9
#endif

// ---------------------------------------------------------------------------
// SD card (SPI1)
// ---------------------------------------------------------------------------
#ifndef SD_SPI_SUPPORT
  #define SD_SPI_SUPPORT
  #define SD_LOW_SPEED  7
  #define SD_HIGH_SPEED 1
  #define SD_SPI        _SPI1
  #define SD_CS_PIN     PD11
#endif
#ifndef SD_CD_PIN
  #define SD_CD_PIN PB15
#endif

// ---------------------------------------------------------------------------
// USB Host (OTG FS)
// ---------------------------------------------------------------------------
#ifndef USB_FLASH_DRIVE_SUPPORT
  #define USB_FLASH_DRIVE_SUPPORT
  #define USE_USB_OTG_FS
#endif

// ---------------------------------------------------------------------------
// Board initialisation — implémentée dans mks_tft28.c
// ---------------------------------------------------------------------------
void MKS_TFT28_Init(void);

#endif