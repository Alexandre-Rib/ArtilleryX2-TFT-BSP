/**
 * @file    res_installer.c
 * @brief   One-shot resource installer: SD /res/pic/ BMP files to W25Q64 (RGB565)
 * @version 2.1
 * @date    Created:       2026-05-30
 *          Last modified: 2026-05-31
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Trigger: presence of a "res/" directory at the root of the SD card.
 *  After a successful install the directory is renamed to "res_cur/" so the
 *  installer does not run again on the next boot.  To reinstall, put a fresh
 *  "res/" directory on the SD (remove or rename the old "res_cur/" first if
 *  needed, as FatFS cannot overwrite a non-empty directory).
 *
 *  BMP format handled:
 *    - 24-bit colour (BI_RGB, no compression)
 *    - Positive or negative height (bottom-to-top or top-to-bottom storage)
 *    - Row padding to 4-byte boundary
 *    - Images smaller than RES_IMG_W x RES_IMG_H are centred with black padding.
 *    - Images larger than the slot are rejected (ERR_DIM).
 *
 *  Flash safety:
 *    Each slot is bounds-checked against SETTINGS_ADDR before erasing or writing.
 *    Resource slots (starting at 0x701000, 6 × 16 KB = up to 0x719000) are well
 *    below the settings sector (0x7FF000) and cannot overwrite it.
 *
 *  Magic sector layout (RES_MAGIC_ADDR = 0x700000):
 *    [0..3]  uint32_t  RES_MAGIC_VALUE  — written last; absent = install incomplete
 *    [4..9]  uint8_t   status[RES_IMG_SLOT_COUNT]  — SlotStatus_t per slot
 */

#include "res_installer.h"
#include "res_map.h"
#include "flash_map.h"
#include "font_embedded.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "mks_tft28.h"
#include "keyboard.h"
#include "xpt2046.h"
#include "ff.h"
#include <string.h>

// Magic sector byte layout (little-endian uint32_t on Cortex-M3):
//   [0..3]  uint32_t  RES_MAGIC_VALUE
//   [4..9]  uint8_t   status[RES_IMG_SLOT_COUNT]
#define MAGIC_VALID_OFFSET 4u

typedef enum {
    SLOT_OK        = 0u,  // installed, exact dimensions
    SLOT_WAR_DIM   = 1u,  // installed with black padding (image smaller than slot)
    SLOT_MISS      = 2u,  // file not found on SD
    SLOT_ERR_FMT   = 3u,  // not a valid 24-bit uncompressed BMP
    SLOT_ERR_DIM   = 4u,  // image larger than slot
    SLOT_ERR_IO    = 5u,  // read/seek error during install
    SLOT_ERR_FLASH = 6u,  // slot address exceeds flash capacity
} SlotStatus_t;

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
    "0:/res/pic/keyboard.bmp",
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
static SlotStatus_t install_bmp(uint8_t slot, FIL *fp)
{
    static uint8_t hdr[54];
    UINT br;
    if (f_read(fp, hdr, 54u, &br) != FR_OK || br < 54u) return SLOT_ERR_IO;
    if (hdr[0] != 'B' || hdr[1] != 'M')                 return SLOT_ERR_FMT;

    uint32_t pixel_offset = LE32(hdr + 10);
    int32_t  raw_w        = (int32_t)LE32(hdr + 18);
    int32_t  raw_h        = (int32_t)LE32(hdr + 22);
    uint16_t bpp          = LE16(hdr + 28);
    uint32_t compression  = LE32(hdr + 30);

    if (bpp != 24u || compression != 0u) return SLOT_ERR_FMT;

    int32_t img_w  = raw_w;
    int32_t img_h  = (raw_h < 0) ? -raw_h : raw_h;
    bool    bot_up = (raw_h > 0);

    if (img_w > (int32_t)RES_IMG_W || img_h > (int32_t)RES_IMG_H) return SLOT_ERR_DIM;

    bool needs_pad = (img_w < (int32_t)RES_IMG_W || img_h < (int32_t)RES_IMG_H);

    uint32_t row_bytes  = (uint32_t)img_w * 3u;
    uint32_t row_stride = (row_bytes + 3u) & ~3u;

    int32_t pad_left  = ((int32_t)RES_IMG_W - img_w) / 2;
    int32_t pad_right = (int32_t)RES_IMG_W - img_w - pad_left;
    int32_t pad_top   = ((int32_t)RES_IMG_H - img_h) / 2;

    s_base_addr   = RES_IMG_ADDR(slot);
    s_byte_offset = 0;
    s_page_pos    = 0;

    for (int32_t vis_row = 0; vis_row < (int32_t)RES_IMG_H; vis_row++) {
        int32_t img_row = vis_row - pad_top;

        if (img_row < 0 || img_row >= img_h) {
            for (int32_t x = 0; x < (int32_t)RES_IMG_W; x++) { page_push(0); page_push(0); }
        } else {
            uint32_t file_row = bot_up ? (uint32_t)(img_h - 1 - img_row) : (uint32_t)img_row;
            uint32_t seek_pos = pixel_offset + file_row * row_stride;

            if (f_lseek(fp, (FSIZE_t)seek_pos) != FR_OK)                          return SLOT_ERR_IO;
            if (f_read(fp, s_row_buf, row_bytes, &br) != FR_OK || br != row_bytes) return SLOT_ERR_IO;

            for (int32_t x = 0; x < pad_left;  x++) { page_push(0); page_push(0); }

            for (int32_t x = 0; x < img_w; x++) {
                uint8_t  b  = s_row_buf[(uint32_t)x * 3u + 0u];
                uint8_t  g  = s_row_buf[(uint32_t)x * 3u + 1u];
                uint8_t  r  = s_row_buf[(uint32_t)x * 3u + 2u];
                uint16_t px = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
                page_push((uint8_t)(px >> 8));
                page_push((uint8_t)(px & 0xFFu));
            }

            for (int32_t x = 0; x < pad_right; x++) { page_push(0); page_push(0); }
        }
    }

    page_flush();
    if (s_byte_offset < RES_IMG_BYTES) return SLOT_ERR_IO;
    return needs_pad ? SLOT_WAR_DIM : SLOT_OK;
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

static SlotStatus_t install_slot(uint8_t slot)
{
    FIL     fp;
    FRESULT fr = f_open(&fp, BMP_PATHS[slot], FA_READ);
    if (fr != FR_OK) return SLOT_MISS;
    SlotStatus_t st = install_bmp(slot, &fp);
    f_close(&fp);
    return st;
}

// ---------------------------------------------------------------------------
// Recursive directory delete (FatFS only removes empty dirs with f_unlink)
// ---------------------------------------------------------------------------
static void rmdir_recursive(const char *path)
{
    // Copy path to a local buffer: path may point to the static child buffer
    // below, which is overwritten during the loop.  Without this copy,
    // f_unlink(path) at the end would unlink the last child, not the directory.
    char path_buf[128];
    size_t plen = strlen(path);
    if (plen >= sizeof(path_buf)) return;
    memcpy(path_buf, path, plen + 1u);

    DIR     dir;
    FILINFO fno;
    static char child[128];

    if (f_opendir(&dir, path_buf) != FR_OK) return;

    for (;;) {
        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == '\0') break;

        size_t nlen = strlen(fno.fname);
        if (plen + 1u + nlen + 1u > sizeof(child)) continue;

        memcpy(child, path_buf, plen);
        child[plen] = '/';
        memcpy(child + plen + 1u, fno.fname, nlen + 1u);

        if (fno.fattrib & AM_DIR)
            rmdir_recursive(child);
        else
            f_unlink(child);
    }

    f_closedir(&dir);
    f_unlink(path_buf);
}

// ---------------------------------------------------------------------------
// Sound slot helpers
// ---------------------------------------------------------------------------
static void erase_snd_slot(uint8_t slot)
{
    FlashMap_EraseSector(RES_SND_ADDR(slot));   // one 4 KB sector
}

// Extract base name of a filename (no extension, uppercase, exactly 8 bytes zero-padded)
static void extract_base_name(const char *fname, char out[8])
{
    memset(out, 0, 8u);
    for (uint8_t i = 0; i < 8u && fname[i] != '.' && fname[i] != '\0'; i++) {
        char c = fname[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        out[i] = c;
    }
}

// True if the slot header matches the given 8-byte base name
static bool slot_name_matches(uint8_t slot, const char base[8])
{
    char hdr[8];
    FlashMap_Read(RES_SND_ADDR(slot), (uint8_t *)hdr, 8u);
    if ((uint8_t)hdr[0] == 0xFFu) return false;   // empty slot
    return memcmp(hdr, base, 8u) == 0;
}

// True if the slot has never been written (all 0xFF)
static bool slot_is_empty(uint8_t slot)
{
    uint8_t first = 0;
    FlashMap_Read(RES_SND_ADDR(slot), &first, 1u);
    return first == 0xFFu;
}

static bool is_snd_file(const FILINFO *fno)
{
    if (fno->fattrib & AM_DIR) return false;
    uint16_t len = (uint16_t)strlen(fno->fname);
    if (len < 4u) return false;
    const char *ext = fno->fname + len - 4u;
    return ext[0] == '.' &&
           (ext[1] == 's' || ext[1] == 'S') &&
           (ext[2] == 'n' || ext[2] == 'N') &&
           (ext[3] == 'd' || ext[3] == 'D');
}

static SlotStatus_t install_snd_slot(uint8_t slot, const char *fname)
{
    // Build path "0:/res/sound/<fname>"
    char path[40];
    memcpy(path, "0:/res/sound/", 13u);
    strncpy(path + 13, fname, sizeof(path) - 14u);
    path[sizeof(path) - 1u] = '\0';

    FIL  fp;
    UINT br;
    if (f_open(&fp, path, FA_READ) != FR_OK) return SLOT_MISS;

    s_base_addr   = RES_SND_ADDR(slot);
    s_byte_offset = 0;
    s_page_pos    = 0;

    // 8-byte name header: filename without extension, uppercase, null-padded
    char name_hdr[8] = {0};
    uint8_t nlen = 0;
    for (uint8_t j = 0; j < 8u && fname[j] != '.' && fname[j] != '\0'; j++) {
        char c = fname[j];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        name_hdr[nlen++] = c;
    }
    for (uint8_t j = 0; j < 8u; j++) page_push((uint8_t)name_hdr[j]);

    // Copy ToneNote_t records until EOF or end marker
    bool has_data = false;
    for (;;) {
        uint8_t tmp[4];
        if (f_read(&fp, tmp, 4u, &br) != FR_OK || br < 4u) break;
        uint16_t freq = (uint16_t)(tmp[0] | ((uint16_t)tmp[1] << 8u));
        if (freq == 0xFFFFu) break;
        if (s_byte_offset + s_page_pos + 8u > RES_SND_SLOT_SIZE) break;
        for (uint8_t j = 0; j < 4u; j++) page_push(tmp[j]);
        has_data = true;
    }
    f_close(&fp);

    page_push(0xFFu); page_push(0xFFu); page_push(0xFFu); page_push(0xFFu);
    page_flush();

    return has_data ? SLOT_OK : SLOT_MISS;
}

// ---------------------------------------------------------------------------
// Progress display
// ---------------------------------------------------------------------------
static void show_label(const char *label)
{
    GUI_FillRectColor(0, (uint16_t)(LCD_HEIGHT/2 + 10),
                      LCD_WIDTH, (uint16_t)(LCD_HEIGHT/2 + 26), BLACK);
    Font_DrawStringCentered(0, LCD_HEIGHT/2 + 10, LCD_WIDTH, LCD_HEIGHT/2 + 26,
                             label, 1, WHITE);
}

static void show_progress(uint8_t step)
{
    int16_t bar_w = (int16_t)(((uint32_t)(step + 1u) * (uint32_t)LCD_WIDTH)
                               / (uint32_t)RES_IMG_SLOT_COUNT);
    GUI_FillRectColor(0, (uint16_t)(LCD_HEIGHT/2 - 4),
                      (uint16_t)bar_w, (uint16_t)(LCD_HEIGHT/2 + 4), 0x07E0u);
    GUI_FillRectColor((uint16_t)bar_w, (uint16_t)(LCD_HEIGHT/2 - 4),
                      LCD_WIDTH, (uint16_t)(LCD_HEIGHT/2 + 4), 0x2104u);
    show_label(SLOT_LABELS[step]);
}

// ---------------------------------------------------------------------------
// Result screen — shows only non-OK slots, scrollable, blocks until screen press
// ---------------------------------------------------------------------------
static void show_result(const uint8_t valid_flags[])
{
    static const struct { const char *label; uint16_t color; } STATUS_INFO[] = {
        { "OK",        0x07E0u },  // SLOT_OK        (never displayed)
        { "WAR_DIM",   0xFD20u },  // SLOT_WAR_DIM   — orange
        { "MISS",      0x8410u },  // SLOT_MISS      — grey
        { "ERR_FMT",   0xF800u },  // SLOT_ERR_FMT   — red
        { "ERR_DIM",   0xF800u },  // SLOT_ERR_DIM   — red
        { "ERR_IO",    0xF800u },  // SLOT_ERR_IO    — red
        { "ERR_FLASH", 0xF800u },  // SLOT_ERR_FLASH — red
    };

    // Collect non-OK slots only
    uint8_t items[RES_IMG_SLOT_COUNT];
    uint8_t n = 0;
    for (uint8_t i = 0; i < RES_IMG_SLOT_COUNT; i++) {
        if (valid_flags[i] != (uint8_t)SLOT_OK)
            items[n++] = i;
    }
    if (n == 0) return;  // all OK — nothing to report

    // Layout
    int16_t area_y0 = 30;
    int16_t area_y1 = 195;
    int16_t item_h  = 20;
    uint8_t visible = (uint8_t)((area_y1 - area_y0) / item_h);  // 8
    int16_t hint_y0 = 205;
    int16_t hint_y1 = 236;

    uint8_t scroll = 0;
    bool    redraw = true;

    // Wait for any current touch to release before listening for new press
    // (PENIRQ active-low: 0 = touched, 1 = released)
    while (!XPT2046_Read_Pen()) { /* idle */ }

    for (;;) {
        if (redraw) {
            redraw = false;
            GUI_Clear(BLACK);
            Font_DrawStringCentered(0, 4, LCD_WIDTH, 22, "INSTALL RESULT", 2, 0x07FFu);
            GUI_FillRectColor(8, 26, LCD_WIDTH - 8, 27, 0x4228u);

            uint8_t show = (uint8_t)(n - scroll);
            if (show > visible) show = visible;

            for (uint8_t r = 0; r < show; r++) {
                uint8_t i  = items[scroll + r];
                uint8_t st = valid_flags[i];
                if (st >= 7u) st = 5u;
                int16_t y = area_y0 + (int16_t)(r * item_h);
                Font_DrawString(8,   y + 4, SLOT_LABELS[i],           1, WHITE);
                Font_DrawString(180, y,     STATUS_INFO[st].label, 2, STATUS_INFO[st].color);
            }

            // Scroll indicators (keyboard UP / DOWN)
            if (scroll > 0)
                Font_DrawStringCentered(LCD_WIDTH - 20, area_y0,
                                         LCD_WIDTH, area_y0 + 16, "^", 2, 0x07FFu);
            if ((uint8_t)(scroll + visible) < n)
                Font_DrawStringCentered(LCD_WIDTH - 20, (int16_t)(area_y1 - 16),
                                         LCD_WIDTH, area_y1,       "v", 2, 0x07FFu);

            // Bottom hint
            GUI_FillRectColor(8, (uint16_t)(hint_y0 - 4),
                              LCD_WIDTH - 8, (uint16_t)(hint_y0 - 3), 0x4228u);
            Font_DrawStringCentered(0, hint_y0, LCD_WIDTH, hint_y1,
                                     "Press screen to continue", 1, 0x8410u);
        }

        // Poll input
        Keyboard_Process();
        if (Keyboard_HasNewKey()) {
            uint8_t key = Keyboard_GetKeycode();
            if      (key == KB_KEY_UP   && scroll > 0)                        { scroll--; redraw = true; }
            else if (key == KB_KEY_DOWN && (uint8_t)(scroll + visible) < n)   { scroll++; redraw = true; }
            else    { return; }  // any other key = continue
        }
        if (!XPT2046_Read_Pen()) { return; }  // PENIRQ low = touch = continue
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool ResInstaller_IsInstalled(void)
{
    uint32_t magic = 0;
    FlashMap_Read(RES_MAGIC_ADDR, (uint8_t *)&magic, 4u);
    return (magic == RES_MAGIC_VALUE);
}

bool ResInstaller_IsSlotValid(uint8_t slot)
{
    if (slot >= RES_IMG_SLOT_COUNT) return false;
    uint32_t magic = 0;
    FlashMap_Read(RES_MAGIC_ADDR, (uint8_t *)&magic, sizeof(magic));
    if (magic != RES_MAGIC_VALUE) return false;
    uint8_t status = 0;
    FlashMap_Read(RES_MAGIC_ADDR + MAGIC_VALID_OFFSET + slot, &status, 1u);
    return (status == (uint8_t)SLOT_OK || status == (uint8_t)SLOT_WAR_DIM);
}

void ResInstaller_ShowResult(void)
{
    uint32_t magic = 0;
    FlashMap_Read(RES_MAGIC_ADDR, (uint8_t *)&magic, 4u);
    if (magic != RES_MAGIC_VALUE) return;

    uint8_t valid_flags[RES_IMG_SLOT_COUNT];
    FlashMap_Read(RES_MAGIC_ADDR + MAGIC_VALID_OFFSET, valid_flags, RES_IMG_SLOT_COUNT);
    show_result(valid_flags);
}

bool ResInstaller_Run(void)
{
    // Only run if the SD card has a fresh "res/" directory at its root.
    if (f_mount(&s_fs, "0:", 1) != FR_OK) return false;

    FILINFO fno;
    if (f_stat("0:/res", &fno) != FR_OK) {
        f_mount(NULL, "0:", 0);
        return false;
    }

    GUI_Clear(BLACK);
    Font_DrawStringCentered(0, 10, LCD_WIDTH, 26, "INSTALLING RESOURCES", 1, 0x07FFu);

    FlashMap_EraseSector(RES_MAGIC_ADDR);  // erase stamp first (power-fail safety)

    uint8_t valid_flags[RES_IMG_SLOT_COUNT];
    for (uint8_t i = 0; i < RES_IMG_SLOT_COUNT; i++) {
        show_progress(i);

        // Bounds check before touching flash — must not reach settings sector
        if (RES_IMG_ADDR(i) + RES_IMG_SLOT_SIZE > SETTINGS_ADDR) {
            valid_flags[i] = (uint8_t)SLOT_ERR_FLASH;
            continue;
        }

        erase_slot(i);
        valid_flags[i] = (uint8_t)install_slot(i);
    }

    // Sound slots -- incremental update: only overwrite slots whose name matches the file.
    // New files occupy the first empty slot. Unmatched slots are left untouched.
    // This preserves sounds that are not in the current res/sound/ directory.
    {
        DIR     dir_snd;
        FILINFO fno_snd;

        if (f_opendir(&dir_snd, "0:/res/sound") == FR_OK) {
            while (1) {
                if (f_readdir(&dir_snd, &fno_snd) != FR_OK || fno_snd.fname[0] == '\0') break;
                if (!is_snd_file(&fno_snd)) continue;

                char base[8];
                extract_base_name(fno_snd.fname, base);

                // Find target slot: prefer existing slot with same name, fallback to first empty
                int8_t target    = -1;
                int8_t first_emp = -1;
                for (uint8_t s = 0; s < RES_SND_SLOT_COUNT; s++) {
                    if (slot_name_matches(s, base)) { target = (int8_t)s; break; }
                    if (first_emp < 0 && slot_is_empty(s)) first_emp = (int8_t)s;
                }
                if (target < 0) target = first_emp;
                if (target < 0) continue;   // no room — skip
                if (RES_SND_ADDR((uint8_t)target) + RES_SND_SLOT_SIZE > SETTINGS_ADDR) continue;

                show_label(fno_snd.fname);
                erase_snd_slot((uint8_t)target);
                install_snd_slot((uint8_t)target, fno_snd.fname);
            }
            f_closedir(&dir_snd);
        }
        // Slots not touched by the loop keep their previous content.
    }

    // Write magic sector: [magic 4B LE][status[6] 6B] = 10 bytes
    static uint8_t hdr_buf[4u + RES_IMG_SLOT_COUNT];
    hdr_buf[0] = (uint8_t)(RES_MAGIC_VALUE);
    hdr_buf[1] = (uint8_t)(RES_MAGIC_VALUE >>  8);
    hdr_buf[2] = (uint8_t)(RES_MAGIC_VALUE >> 16);
    hdr_buf[3] = (uint8_t)(RES_MAGIC_VALUE >> 24);
    for (uint8_t i = 0; i < RES_IMG_SLOT_COUNT; i++)
        hdr_buf[MAGIC_VALID_OFFSET + i] = valid_flags[i];
    FlashMap_Write(RES_MAGIC_ADDR, hdr_buf, (uint32_t)sizeof(hdr_buf));

    // Rename "res" → "res_cur" to prevent reinstall on next boot.
    // Remove any existing "res_cur" first (FatFS cannot overwrite non-empty dirs).
    FILINFO fno_cur;
    if (f_stat("0:/res_cur", &fno_cur) == FR_OK)
        rmdir_recursive("0:/res_cur");
    f_rename("0:/res", "0:/res_cur");

    f_mount(NULL, "0:", 0);
    return true;
}
