/**
 * @file    res_installer.c
 * @brief   One-shot resource installer: SD /res/pic/ BMP files to W25Q64 (RGB565)
 * @version 1.0
 * @date    Created:       2026-05-30
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  BMP 24-bit, any standard tool (GIMP, Paint, Photoshop): File → Export/Save as → BMP.
 *  No command-line conversion needed.
 *
 *  BMP format handled:
 *    - 24-bit colour (BI_RGB, no compression)
 *    - Positive height (bottom-to-top storage, standard) or negative (top-to-bottom)
 *    - Row padding to 4-byte boundary
 *    - Pixel data: BGR order (standard BMP) → converted to RGB565 big-endian for LCD
 *
 *  Expected SD layout:
 *    /res/pic/picture.bmp
 *    /res/pic/animation.bmp
 *    /res/pic/sound.bmp
 *    /res/pic/calibration.bmp
 *    /res/pic/undef_menu.bmp
 *
 *  Images must be exactly RES_IMG_W × RES_IMG_H pixels.
 *
 *  Magic sector layout (RES_MAGIC_ADDR, first 4 KB of RES_BASE_ADDR):
 *    [0..3]  uint32_t  RES_MAGIC_VALUE  — written last; absent = install incomplete
 *    [4..8]  uint8_t   valid[5]         — 1 = slot OK, 0 = file not found / error
 */

#include "res_installer.h"
#include "res_map.h"
#include "flash_map.h"
#include "font_embedded.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "mks_tft28.h"
#include "ff.h"
#include <string.h>

// Magic sector byte layout:
//   [0..3]  uint32_t  RES_MAGIC_VALUE
//   [4..7]  uint32_t  firmware fingerprint (XOR of first 512 B of MCU app flash)
//   [8..13] uint8_t   valid[RES_IMG_SLOT_COUNT]
#define MAGIC_FP_OFFSET    4u
#define MAGIC_VALID_OFFSET 8u

// XOR fingerprint of the first 512 bytes of the application in MCU flash.
// Changes whenever a new binary is flashed; stable across reboots.
static uint32_t firmware_fingerprint(void)
{
    const uint32_t *flash = (const uint32_t *)0x08007000u;  // app start (after bootloader)
    uint32_t fp = 0;
    for (uint32_t i = 0; i < 128u; i++)   // 128 x 4 B = 512 B
        fp ^= flash[i];
    return fp;
}

// Read little-endian integers from a byte buffer (no alignment requirement)
#define LE16(p) ((uint16_t)((p)[0] | ((uint16_t)(p)[1] << 8)))
#define LE32(p) ((uint32_t)((p)[0] | ((uint32_t)(p)[1]<<8) | \
                             ((uint32_t)(p)[2]<<16) | ((uint32_t)(p)[3]<<24)))

// ---------------------------------------------------------------------------
// SD file paths
// ---------------------------------------------------------------------------
static const char * const BMP_PATHS[RES_IMG_SLOT_COUNT] = {
    "0:/res/pic/picture.bmp",
    "0:/res/pic/animation.bmp",
    "0:/res/pic/sound.bmp",
    "0:/res/pic/calibration.bmp",
    "0:/res/pic/undef_menu.bmp",
    "0:/res/pic/keyboard.bmp",   // intentionally missing → slot stays invalid
};
static const char * const SLOT_LABELS[RES_IMG_SLOT_COUNT] = {
    "picture", "animation", "sound", "calibration", "undef", "keyboard",
};

// ---------------------------------------------------------------------------
// Static buffers
// ---------------------------------------------------------------------------
static FATFS   s_fs;
static uint8_t s_row_buf[RES_IMG_W * 3u];  // 240 B: one BMP row in BGR24
static uint8_t s_page_buf[FLASH_PAGE_SIZE]; // 256 B: W25Q64 write accumulator

static uint32_t s_base_addr;
static uint32_t s_byte_offset;
static uint16_t s_page_pos;

// ---------------------------------------------------------------------------
// W25Q64 page accumulator
// ---------------------------------------------------------------------------
static void page_push(uint8_t b)
{
    s_page_buf[s_page_pos++] = b;
    if (s_page_pos >= FLASH_PAGE_SIZE) {
        FlashMap_Write(s_base_addr + s_byte_offset, s_page_buf, FLASH_PAGE_SIZE);
        s_byte_offset += FLASH_PAGE_SIZE;
        s_page_pos     = 0;
    }
}

static void page_flush(void)
{
    if (s_page_pos > 0) {
        FlashMap_Write(s_base_addr + s_byte_offset, s_page_buf,
                       (uint32_t)s_page_pos);
        s_byte_offset += s_page_pos;
        s_page_pos     = 0;
    }
}

// ---------------------------------------------------------------------------
// BMP decode + write
// ---------------------------------------------------------------------------
static bool install_bmp(uint8_t slot, FIL *fp)
{
    // BMP file header (14 B) + BITMAPINFOHEADER (40 B) = 54 B minimum
    static uint8_t hdr[54];
    UINT br;
    if (f_read(fp, hdr, 54u, &br) != FR_OK || br < 54u) return false;
    if (hdr[0] != 'B' || hdr[1] != 'M') return false;

    uint32_t pixel_offset = LE32(hdr + 10);
    int32_t  raw_w        = (int32_t)LE32(hdr + 18);
    int32_t  raw_h        = (int32_t)LE32(hdr + 22);
    uint16_t bpp          = LE16(hdr + 28);
    uint32_t compression  = LE32(hdr + 30);

    if (bpp != 24u || compression != 0u) return false;
    if (raw_w != (int32_t)RES_IMG_W)    return false;

    int32_t  height   = (raw_h < 0) ? -raw_h : raw_h;
    bool     bot_up   = (raw_h > 0);  // positive height = bottom-to-top storage
    uint32_t row_bytes  = (uint32_t)raw_w * 3u;
    uint32_t row_stride = (row_bytes + 3u) & ~3u;  // pad to 4 B

    if (height != (int32_t)RES_IMG_H) return false;

    // Init W25Q64 page accumulator for this slot
    s_base_addr   = RES_IMG_ADDR(slot);
    s_byte_offset = 0;
    s_page_pos    = 0;

    for (int32_t vis_row = 0; vis_row < height; vis_row++) {
        // Visual row 0 = top; BMP positive-height stores bottom row first
        uint32_t file_row = bot_up ? (uint32_t)(height - 1 - vis_row)
                                    : (uint32_t)vis_row;
        uint32_t seek_pos = pixel_offset + file_row * row_stride;

        if (f_lseek(fp, (FSIZE_t)seek_pos) != FR_OK) return false;
        if (f_read(fp, s_row_buf, row_bytes, &br) != FR_OK || br != row_bytes)
            return false;

        // Convert BGR → RGB565 big-endian, push to page accumulator
        for (uint32_t x = 0; x < (uint32_t)raw_w; x++) {
            uint8_t  b  = s_row_buf[x * 3u + 0u];
            uint8_t  g  = s_row_buf[x * 3u + 1u];
            uint8_t  r  = s_row_buf[x * 3u + 2u];
            uint16_t px = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
            page_push((uint8_t)(px >> 8));
            page_push((uint8_t)(px & 0xFFu));
        }
    }

    page_flush();
    return (s_byte_offset >= RES_IMG_BYTES);
}

// ---------------------------------------------------------------------------
// Slot management
// ---------------------------------------------------------------------------
static void erase_slot(uint8_t slot)
{
    uint32_t base = RES_IMG_ADDR(slot);
    for (uint32_t off = 0; off < RES_IMG_SLOT_SIZE; off += W25QXX_SECTOR_SIZE)
        FlashMap_EraseSector(base + off);
}

static bool install_slot(uint8_t slot)
{
    FIL     fp;
    FRESULT fr = f_open(&fp, BMP_PATHS[slot], FA_READ);
    if (fr != FR_OK) return false;
    bool ok = install_bmp(slot, &fp);
    f_close(&fp);
    return ok;
}

// ---------------------------------------------------------------------------
// Progress display
// ---------------------------------------------------------------------------
static void show_progress(uint8_t step)
{
    int16_t bar_w = (int16_t)(((uint32_t)(step + 1u) * (uint32_t)LCD_WIDTH)
                               / (uint32_t)RES_IMG_SLOT_COUNT);
    GUI_FillRectColor(0, (uint16_t)(LCD_HEIGHT/2 - 4),
                      (uint16_t)bar_w, (uint16_t)(LCD_HEIGHT/2 + 4), 0x07E0u);
    GUI_FillRectColor((uint16_t)bar_w, (uint16_t)(LCD_HEIGHT/2 - 4),
                      LCD_WIDTH, (uint16_t)(LCD_HEIGHT/2 + 4), 0x2104u);
    // Clear the label area before writing to avoid text overlap between steps
    GUI_FillRectColor(0, (uint16_t)(LCD_HEIGHT/2 + 10),
                      LCD_WIDTH, (uint16_t)(LCD_HEIGHT/2 + 26), BLACK);
    Font_DrawStringCentered(0, LCD_HEIGHT/2 + 10, LCD_WIDTH, LCD_HEIGHT/2 + 26,
                             SLOT_LABELS[step], 1, WHITE);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool ResInstaller_IsInstalled(void)
{
    uint32_t magic = 0;
    FlashMap_Read(RES_MAGIC_ADDR, (uint8_t *)&magic, 4u);
    if (magic != RES_MAGIC_VALUE) return false;

    uint32_t stored_fp = 0;
    FlashMap_Read(RES_MAGIC_ADDR + MAGIC_FP_OFFSET, (uint8_t *)&stored_fp, 4u);
    return (stored_fp == firmware_fingerprint());
}

bool ResInstaller_IsSlotValid(uint8_t slot)
{
    if (slot >= RES_IMG_SLOT_COUNT) return false;
    // Check magic presence only — slot data may survive a firmware update
    // if the SD was absent at the time of the new firmware's first boot.
    uint32_t magic = 0;
    FlashMap_Read(RES_MAGIC_ADDR, (uint8_t *)&magic, sizeof(magic));
    if (magic != RES_MAGIC_VALUE) return false;
    uint8_t valid = 0;
    FlashMap_Read(RES_MAGIC_ADDR + MAGIC_VALID_OFFSET + slot, &valid, 1u);
    return (valid == 1u);
}

void ResInstaller_Run(void)
{
    // Skip if already installed for this exact firmware build.
    // A new binary has a different __DATE__ "__TIME__" stamp → triggers reinstall.
    if (ResInstaller_IsInstalled()) return;

    // Mount SD — if absent we cannot install.
    // The existing W25Q64 image data (from a previous install) remains readable
    // via IsSlotValid() even when IsInstalled() returns false.
    if (f_mount(&s_fs, "0:", 1) != FR_OK) return;

    FILINFO fno;
    if (f_stat("0:/res/pic", &fno) != FR_OK) {
        f_mount(NULL, "0:", 0);
        return;
    }

    GUI_Clear(BLACK);
    Font_DrawStringCentered(0, 10, LCD_WIDTH, 26,
                             "INSTALLING RESOURCES", 1, 0x07FFu);

    FlashMap_EraseSector(RES_MAGIC_ADDR);   // erase stamp first (power-fail safety)

    uint8_t valid_flags[RES_IMG_SLOT_COUNT];
    for (uint8_t i = 0; i < RES_IMG_SLOT_COUNT; i++) {
        show_progress(i);
        erase_slot(i);
        valid_flags[i] = install_slot(i) ? 1u : 0u;
    }

    // Magic sector: [magic 4B][fingerprint 4B][valid[6] 6B] = 14 bytes
    static uint8_t hdr_buf[4u + 4u + RES_IMG_SLOT_COUNT];
    uint32_t fp = firmware_fingerprint();
    hdr_buf[0] = (uint8_t)(RES_MAGIC_VALUE >> 24);
    hdr_buf[1] = (uint8_t)(RES_MAGIC_VALUE >> 16);
    hdr_buf[2] = (uint8_t)(RES_MAGIC_VALUE >>  8);
    hdr_buf[3] = (uint8_t)(RES_MAGIC_VALUE);
    hdr_buf[4] = (uint8_t)(fp >> 24);
    hdr_buf[5] = (uint8_t)(fp >> 16);
    hdr_buf[6] = (uint8_t)(fp >>  8);
    hdr_buf[7] = (uint8_t)(fp);
    for (uint8_t i = 0; i < RES_IMG_SLOT_COUNT; i++)
        hdr_buf[MAGIC_VALID_OFFSET + i] = valid_flags[i];
    FlashMap_Write(RES_MAGIC_ADDR, hdr_buf, (uint32_t)sizeof(hdr_buf));

    f_mount(NULL, "0:", 0);

    Font_DrawStringCentered(0, LCD_HEIGHT/2 + 30, LCD_WIDTH, LCD_HEIGHT/2 + 46,
                             "DONE", 1, 0x07E0u);
}
