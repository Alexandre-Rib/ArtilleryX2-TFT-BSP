/**
 * @file    scene_image.c
 * @brief   Scene: BMP image viewer — SD browser + Flash icon slots
 * @version 2.0
 * @date    Created: 2026-05-31
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Two sub-modes:
 *    BROWSE  — source selector [SD][FLASH], FileBrowserWidget, footer [BACK|VIEW]
 *    DISPLAY — image rendered scan-line by scan-line, footer [PREV|BACK|NEXT]
 *
 *  Browse layout (320×240):
 *    y=  0.. 19 : title "IMAGE"
 *    y= 20.. 47 : source selector [SD][FLASH]
 *    y= 48.. 49 : separator
 *    y= 50..196 : file list 7 rows × 21 px
 *    y=197..212 : status bar
 *    y=213      : gap
 *    y=214..239 : footer [BACK | VIEW]
 *
 *  Display layout (320×240):
 *    y=  0..239 : image — fills the full screen (scale-to-fit if needed)
 *    y=204..239 : [PREV][BACK(esc)|NEXT] footer overlaid on the bottom of the image
 *
 *  SD source  : .bmp files browsed via SdBrowser (any depth).
 *  Flash source: installed image slots in W25Q64 (80×80 icons, centred).
 *
 *  BMP formats accepted (SD):
 *    • 24-bit uncompressed BGR (compression=0)
 *    • 16-bit BI_BITFIELDS RGB565 (compression=3, masks F800/07E0/001F)
 *  Any format ≠ the above shows "UNSUPPORTED FORMAT".
 *  Images ≤ 320×DISP_IMG_H : displayed at native size, centred.
 *  Images > display area    : scaled to fit (aspect-ratio preserved, nearest-neighbour).
 *  Row stride > BMP_MAX_STRIDE bytes : "IMAGE TOO LARGE".
 */

#include "scene_image.h"
#include "ui_nav.h"
#include "sd_browser.h"
#include "ui_filebrowser.h"
#include "res_map.h"
#include "res_installer.h"
#include "img_draw.h"
#include "demo_app.h"
#include "font_embedded.h"
#include "ui_nav.h"
#include "GUI.h"
#include "LCD_Init.h"
#include "LCD_Colors.h"
#include "os_timer.h"
#include "ff.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Browse layout — mirrors sound scene (same selector + row geometry)
// ---------------------------------------------------------------------------
#define BR_TITLE_H    20
#define BR_SEL_H      28
#define BR_SEP_H       2
#define BR_LIST_ROWS   7
#define BR_ROW_H      21
#define BR_STATUS_H   16
#define BR_FOOTER_H   26

#define BR_TITLE_Y0    0
#define BR_TITLE_Y1    BR_TITLE_H
#define BR_SEL_Y0      BR_TITLE_Y1
#define BR_SEL_Y1     (BR_SEL_Y0  + BR_SEL_H)
#define BR_SEP_Y0      BR_SEL_Y1
#define BR_SEP_Y1     (BR_SEP_Y0  + BR_SEP_H)
#define BR_LIST_Y0     BR_SEP_Y1
#define BR_LIST_Y1    (BR_LIST_Y0 + BR_LIST_ROWS * BR_ROW_H)
#define BR_STATUS_Y0   BR_LIST_Y1
#define BR_STATUS_Y1  (BR_STATUS_Y0 + BR_STATUS_H)
#define BR_FOOTER_Y0  (LCD_HEIGHT - BR_FOOTER_H)

#define BR_BTN_W       100
#define BR_BTN_H        20
#define BR_BTN_GAP       8
#define BR_BTN_Y0      (BR_SEL_Y0 + (BR_SEL_H - BR_BTN_H) / 2)
#define BR_BTN_Y1      (BR_BTN_Y0 + BR_BTN_H)
#define BR_BTN_SD_X0   ((LCD_WIDTH - 2 * BR_BTN_W - BR_BTN_GAP) / 2)
#define BR_BTN_SD_X1   (BR_BTN_SD_X0 + BR_BTN_W)
#define BR_BTN_FL_X0   (BR_BTN_SD_X1 + BR_BTN_GAP)
#define BR_BTN_FL_X1   (BR_BTN_FL_X0 + BR_BTN_W)

#define BR_BACK_X0     0
#define BR_BACK_X1   158
#define BR_VIEW_X0   162
#define BR_VIEW_X1   LCD_WIDTH

static const FBWidgetGeom_t k_list_geom = {
    0, BR_LIST_Y0, LCD_WIDTH, BR_LIST_ROWS, BR_ROW_H
};

// ---------------------------------------------------------------------------
// Display layout
// ---------------------------------------------------------------------------
#define DISP_FOOT_H   36
#define DISP_IMG_H   (LCD_HEIGHT - DISP_FOOT_H)   // 204 px — footer is NOT overlaid
#define DISP_FOOT_Y0  DISP_IMG_H

#define DISP_PREV_X0    0
#define DISP_PREV_X1  106
#define DISP_BACK_X0  107
#define DISP_BACK_X1  213
#define DISP_NEXT_X0  214
#define DISP_NEXT_X1  LCD_WIDTH

// Flash icon centred in 320×DISP_IMG_H display area
#define FLASH_ICON_X  ((LCD_WIDTH  - (int16_t)RES_IMG_W) / 2)
#define FLASH_ICON_Y  ((DISP_IMG_H - (int16_t)RES_IMG_H) / 2)

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------
#define C_TITLE_BG    0x000Fu
#define C_TITLE_FG    0xFFFFu
#define C_SEL_BG      0x0821u
#define C_SEP         0x07FFu
#define C_BTN_ACT_BG  0x000Fu
#define C_BTN_ACT_FG  0xFFE0u
#define C_BTN_ACT_BD  0x07FFu
#define C_BTN_IDL_BG  0x2104u
#define C_BTN_IDL_FG  0x4208u
#define C_STATUS_BG   0x0821u
#define C_STATUS_IDLE 0x4208u
#define C_STATUS_SEL  0x8410u
#define C_STATUS_ERR  0xF800u
#define C_BACK_BG     0x8800u
#define C_BACK_FG     0xFFFFu
#define C_ACT_BTN     0x0400u
#define C_DIS_BTN     0x2104u
#define C_ACT_FG      0xFFFFu
#define C_DIS_FG      0x4208u
#define C_VIEW_RDY    0x0400u
#define C_VIEW_IDL    0x2104u

// Les noms sont lus depuis les headers flash lors de flash_scan()
// (s_flash_names remplace l'ancien tableau k_slot_names statique)

// ---------------------------------------------------------------------------
// Sub-mode and source
// ---------------------------------------------------------------------------
typedef enum { IMG_BROWSE, IMG_DISPLAY } ImgMode_t;
typedef enum { SOURCE_SD, SOURCE_FLASH } ImgSource_t;

static ImgMode_t   s_mode;
static ImgSource_t s_source;

// browse
static int8_t s_selected;
static int8_t s_scroll;

// flash slot scan — noms lus depuis les headers au moment du scan
static uint8_t s_flash_slots[RES_IMG_MAX_SLOTS];
static char    s_flash_names[RES_IMG_MAX_SLOTS][RES_IMG_NAME_LEN + 1u];
static uint8_t s_flash_count;

// display
static char     s_disp_name[SDBROW_NAME_LEN];
static int8_t   s_disp_idx;
static int8_t   s_prev_idx;
static int8_t   s_next_idx;
static uint32_t s_disp_ready_ms; // navigation ignored before this timestamp

// BMP row buffer — static, sized for images up to ~1365px wide (24-bit)
#define BMP_MAX_STRIDE  4096u
static uint8_t s_row_buf[BMP_MAX_STRIDE];

static const char *const s_bmp_exts[] = {"bmp"};

// ---------------------------------------------------------------------------
// Flash: scan installed image slots
// ---------------------------------------------------------------------------
static void flash_scan(void)
{
    s_flash_count = 0;
    for (uint8_t i = 0; i < (uint8_t)RES_IMG_MAX_SLOTS && s_flash_count < (uint8_t)RES_IMG_MAX_SLOTS; i++) {
        if (!ResInstaller_IsSlotValid(i)) continue;
        s_flash_slots[s_flash_count] = i;
        ResInstaller_GetSlotName(i, s_flash_names[s_flash_count]);
        s_flash_count++;
    }
}

// ---------------------------------------------------------------------------
// List helpers
// ---------------------------------------------------------------------------
static int8_t list_count(void)
{
    return (s_source == SOURCE_FLASH) ? (int8_t)s_flash_count
                                      : (int8_t)SdBrowser_GetItemCount();
}

static uint8_t build_items(FBWidgetItem_t *items)
{
    int8_t n = list_count();
    if (n <= 0) return 0;
    if (s_source == SOURCE_FLASH) {
        for (uint8_t i = 0; i < (uint8_t)n; i++) {
            items[i].type       = FBWI_FILE;
            items[i].name       = s_flash_names[i];
            items[i].is_playing = false;
        }
    } else {
        for (uint8_t i = 0; i < (uint8_t)n; i++) {
            const SdBrowItem_t *it = SdBrowser_GetItem(i);
            items[i].type       = (FBWidgetItemType_t)it->type;
            items[i].name       = it->name;
            items[i].is_playing = false;
        }
    }
    return (uint8_t)n;
}

static bool selected_is_viewable(void)
{
    if (s_selected < 0) return false;
    if (s_source == SOURCE_FLASH) return true;  // all flash slots are viewable
    const SdBrowItem_t *it = SdBrowser_GetItem((uint8_t)s_selected);
    return it && it->type == SDBROW_ITEM_FILE;
}

// ---------------------------------------------------------------------------
// Navigation helpers (display mode)
// ---------------------------------------------------------------------------
static int8_t disp_find_prev(int8_t from)
{
    if (s_source == SOURCE_FLASH) {
        return (from > 0) ? (int8_t)(from - 1) : -1;
    }
    for (int8_t i = from - 1; i >= 0; i--) {
        const SdBrowItem_t *it = SdBrowser_GetItem((uint8_t)i);
        if (it && it->type == SDBROW_ITEM_FILE) return i;
    }
    return -1;
}

static int8_t disp_find_next(int8_t from)
{
    if (s_source == SOURCE_FLASH) {
        return (from + 1 < (int8_t)s_flash_count) ? (int8_t)(from + 1) : -1;
    }
    uint8_t n = SdBrowser_GetItemCount();
    for (uint8_t i = (uint8_t)(from + 1); i < n; i++) {
        const SdBrowItem_t *it = SdBrowser_GetItem(i);
        if (it && it->type == SDBROW_ITEM_FILE) return (int8_t)i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// BMP display (SD) — nearest-neighbour vertical scale 240→DISP_IMG_H
// ---------------------------------------------------------------------------
typedef enum {
    BMP_OK,
    BMP_ERR_OPEN,
    BMP_ERR_HDR,
    BMP_ERR_FMT,
    BMP_ERR_LARGE,
} BmpRc_t;

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1]<<8)); }

static BmpRc_t bmp_stream(const char *name)
{
    char path[SDBROW_PATH_LEN + SDBROW_NAME_LEN];
    uint16_t plen = (uint16_t)strlen(SdBrowser_GetPath());
    uint16_t nlen = (uint16_t)strlen(name);
    if (plen + nlen >= sizeof(path)) return BMP_ERR_OPEN;
    memcpy(path, SdBrowser_GetPath(), plen);
    memcpy(path + plen, name, nlen + 1u);

    FIL  fp;
    UINT br;
    if (f_open(&fp, path, FA_READ) != FR_OK) return BMP_ERR_OPEN;

    // ---- Parse header -------------------------------------------------------
    uint8_t hdr[54];
    if (f_read(&fp, hdr, 54, &br) != FR_OK || br != 54) { f_close(&fp); return BMP_ERR_HDR; }
    if (hdr[0] != 'B' || hdr[1] != 'M')                 { f_close(&fp); return BMP_ERR_HDR; }

    uint32_t data_off  = le32(hdr + 10);
    uint32_t src_w_u   = le32(hdr + 18);
    int32_t  h_signed  = (int32_t)le32(hdr + 22);
    uint16_t bpp       = le16(hdr + 28);
    uint32_t compr     = le32(hdr + 30);

    bool     bottom_up = (h_signed > 0);
    uint32_t src_h_u   = bottom_up ? (uint32_t)h_signed : (uint32_t)(-h_signed);

    // Sanity: reject unreasonable dimensions
    if (src_w_u == 0 || src_h_u == 0 || src_w_u > 8192u || src_h_u > 8192u) {
        f_close(&fp); return BMP_ERR_HDR;
    }
    uint16_t src_w = (uint16_t)src_w_u;
    uint16_t src_h = (uint16_t)src_h_u;

    // Format checks
    if (bpp != 24u && bpp != 16u)   { f_close(&fp); return BMP_ERR_FMT; }
    if (bpp == 24u && compr != 0u)  { f_close(&fp); return BMP_ERR_FMT; }
    if (bpp == 16u && compr != 3u)  { f_close(&fp); return BMP_ERR_FMT; }

    if (bpp == 16u) {
        uint8_t masks[12];
        if (f_lseek(&fp, 54) != FR_OK ||
            f_read(&fp, masks, 12, &br) != FR_OK || br != 12) {
            f_close(&fp); return BMP_ERR_HDR;
        }
        if (le32(masks)   != 0xF800u ||
            le32(masks+4) != 0x07E0u ||
            le32(masks+8) != 0x001Fu) { f_close(&fp); return BMP_ERR_FMT; }
    }

    uint32_t stride = ((uint32_t)src_w * bpp / 8u + 3u) & ~3u;
    if (stride > BMP_MAX_STRIDE) { f_close(&fp); return BMP_ERR_LARGE; }

    // ---- Compute display rectangle (scale-to-fit, centre) -------------------
    uint16_t disp_w, disp_h;

    if (src_w <= 320u && src_h <= DISP_IMG_H) {
        // Fits native: centre without scaling
        disp_w = src_w;
        disp_h = src_h;
    } else {
        // Scale to fit 320×DISP_IMG_H, preserve aspect ratio
        // Compare scale factors: if 320/src_w <= DISP_IMG_H/src_h → width-limited
        if ((uint32_t)320u * src_h <= (uint32_t)DISP_IMG_H * src_w) {
            // Width-limited: fit to 320
            disp_w = 320u;
            disp_h = (uint16_t)((uint32_t)src_h * 320u / src_w);
            if (disp_h == 0u) disp_h = 1u;
        } else {
            // Height-limited: fit to DISP_IMG_H
            disp_h = DISP_IMG_H;
            disp_w = (uint16_t)((uint32_t)src_w * DISP_IMG_H / src_h);
            if (disp_w == 0u) disp_w = 1u;
        }
    }

    uint16_t x_off = (uint16_t)((320u - disp_w) / 2u);
    uint16_t y_off = (uint16_t)((DISP_IMG_H - disp_h) / 2u);

    // ---- Render — ONE seek then fully sequential reads ----------------------
    //
    // Reading bottom-up BMP top-to-bottom on screen would require seeking
    // backwards through the file (FatFS re-traverses the FAT chain from the
    // start each time → very slow).
    //
    // Instead: iterate file rows 0→src_h-1 (always forward = fast),
    // compute which display rows each source row maps to, and set the LCD
    // window individually per display row.  All f_reads are sequential.
    //
    // For a 320×240 BMP this means 240 sequential reads instead of 204
    // random-access backward seeks → typically 10× faster.

    GUI_FillRectColor(0, 0, LCD_WIDTH, DISP_IMG_H, BLACK);

    if (f_lseek(&fp, data_off) != FR_OK) { f_close(&fp); return BMP_OK; }

    for (uint16_t file_row = 0; file_row < src_h; file_row++) {
        // Keep USB alive: yield every 16 rows (~16 × 1ms = 16 ms intervals)
        if ((file_row & 0x0Fu) == 0u && BSP_YieldHook) BSP_YieldHook();

        if (f_read(&fp, s_row_buf, (UINT)stride, &br) != FR_OK || br != (UINT)stride) break;

        // Map file row → source row (0 = top of image)
        uint16_t src_row = bottom_up ? (uint16_t)(src_h - 1u - file_row) : file_row;

        // Which display rows does this source row feed?
        uint16_t y_start = (uint16_t)((uint32_t)src_row * disp_h / src_h);
        uint16_t y_end   = (uint16_t)((uint32_t)(src_row + 1u) * disp_h / src_h);

        for (uint16_t y_out = y_start; y_out < y_end; y_out++) {
            uint16_t abs_y = (uint16_t)(y_off + y_out);
            LCD_SetWindow(x_off, abs_y,
                          (uint16_t)(x_off + disp_w - 1u), abs_y);

            for (uint16_t x_out = 0; x_out < disp_w; x_out++) {
                uint32_t x_src = (uint32_t)x_out * src_w / disp_w;
                uint16_t px;
                if (bpp == 24u) {
                    uint8_t b = s_row_buf[x_src * 3u];
                    uint8_t g = s_row_buf[x_src * 3u + 1u];
                    uint8_t r = s_row_buf[x_src * 3u + 2u];
                    px = ((uint16_t)(r & 0xF8u) << 8) |
                         ((uint16_t)(g & 0xFCu) << 3) |
                         (b >> 3u);
                } else {
                    px = (uint16_t)s_row_buf[x_src * 2u] |
                         ((uint16_t)s_row_buf[x_src * 2u + 1u] << 8);
                }
                LCD_WR_16BITS_DATA(px);
            }
        }
    }

    f_close(&fp);
    return BMP_OK;
}

// ---------------------------------------------------------------------------
// Draw: source selector
// ---------------------------------------------------------------------------
static void draw_source_btn(int16_t x0, int16_t x1, const char *label, bool active)
{
    uint16_t bg = active ? C_BTN_ACT_BG : C_BTN_IDL_BG;
    uint16_t fg = active ? C_BTN_ACT_FG : C_BTN_IDL_FG;
    GUI_FillRectColor((uint16_t)x0, BR_BTN_Y0, (uint16_t)x1, BR_BTN_Y1, bg);
    if (active) {
        GUI_FillRectColor((uint16_t)x0,    BR_BTN_Y0,   (uint16_t)x1,     BR_BTN_Y0+1, C_BTN_ACT_BD);
        GUI_FillRectColor((uint16_t)x0,    BR_BTN_Y1-1, (uint16_t)x1,     BR_BTN_Y1,   C_BTN_ACT_BD);
        GUI_FillRectColor((uint16_t)x0,    BR_BTN_Y0,   (uint16_t)(x0+1), BR_BTN_Y1,   C_BTN_ACT_BD);
        GUI_FillRectColor((uint16_t)(x1-1),BR_BTN_Y0,   (uint16_t)x1,     BR_BTN_Y1,   C_BTN_ACT_BD);
    }
    Font_DrawStringCentered(x0, BR_BTN_Y0, x1, BR_BTN_Y1, label, 1, fg);
}

static void draw_source_selector(void)
{
    GUI_FillRectColor(0, BR_SEL_Y0, LCD_WIDTH, BR_SEL_Y1, C_SEL_BG);
    draw_source_btn(BR_BTN_SD_X0, BR_BTN_SD_X1, "SD",    s_source == SOURCE_SD);
    draw_source_btn(BR_BTN_FL_X0, BR_BTN_FL_X1, "FLASH", s_source == SOURCE_FLASH);
}

// ---------------------------------------------------------------------------
// Draw: status bar
// ---------------------------------------------------------------------------
static void draw_status(void)
{
    GUI_FillRectColor(0, BR_STATUS_Y0, LCD_WIDTH, BR_STATUS_Y1, C_STATUS_BG);
    const char *msg; uint16_t col;

    if (s_source == SOURCE_FLASH) {
        if (s_flash_count == 0)    { msg = "NO IMAGES IN FLASH"; col = C_STATUS_IDLE; }
        else if (s_selected >= 0)  { msg = s_flash_names[(uint8_t)s_selected]; col = C_STATUS_SEL; }
        else                       { msg = "SELECT + VIEW TO DISPLAY"; col = C_STATUS_IDLE; }
    } else {
        if (!SdBrowser_IsMounted()) {
            msg = "NO SD CARD"; col = C_STATUS_IDLE;
        } else {
            static char dbg[40];
            uint8_t n  = SdBrowser_GetItemCount();
            uint8_t rc = (uint8_t)SdBrowser_GetLastError();
            dbg[0]='['; dbg[1]='0'+n/10; dbg[2]='0'+n%10;
            dbg[3]=' '; dbg[4]='R'; dbg[5]='0'+rc/10; dbg[6]='0'+rc%10;
            dbg[7]=']'; dbg[8]=' ';
            strncpy(dbg + 9, SdBrowser_GetPath() + 1, 30);
            dbg[39] = '\0';
            msg = dbg; col = (SdBrowser_GetLastError() == 0) ? C_STATUS_IDLE : C_STATUS_ERR;
            if (s_selected >= 0) {
                const SdBrowItem_t *it = SdBrowser_GetItem((uint8_t)s_selected);
                if (it && it->type == SDBROW_ITEM_FILE) { msg = it->name; col = C_STATUS_SEL; }
            }
        }
    }
    Font_DrawStringCentered(0, BR_STATUS_Y0, LCD_WIDTH, BR_STATUS_Y1, msg, 1, col);
}

// ---------------------------------------------------------------------------
// Draw: browse footer
// ---------------------------------------------------------------------------
static void draw_browse_footer(void)
{
    GUI_FillRectColor(BR_BACK_X0, BR_FOOTER_Y0, BR_BACK_X1, LCD_HEIGHT, C_BACK_BG);
    Font_DrawStringCentered(BR_BACK_X0, (int16_t)BR_FOOTER_Y0, BR_BACK_X1,
                            LCD_HEIGHT, "BACK", 1, C_BACK_FG);
    GUI_FillRectColor(BR_BACK_X1, BR_FOOTER_Y0, BR_VIEW_X0, LCD_HEIGHT, BLACK);

    bool can = selected_is_viewable();
    GUI_FillRectColor(BR_VIEW_X0, BR_FOOTER_Y0, BR_VIEW_X1, LCD_HEIGHT,
                      can ? C_VIEW_RDY : C_VIEW_IDL);
    Font_DrawStringCentered(BR_VIEW_X0, (int16_t)BR_FOOTER_Y0, BR_VIEW_X1,
                            LCD_HEIGHT, "VIEW", 1, can ? C_ACT_FG : C_DIS_FG);
}

// ---------------------------------------------------------------------------
// Draw: display footer
// ---------------------------------------------------------------------------
static void draw_disp_footer(void)
{
    bool hp = (s_prev_idx >= 0);
    bool hn = (s_next_idx >= 0);

    GUI_FillRectColor(DISP_PREV_X0, DISP_FOOT_Y0, DISP_PREV_X1, LCD_HEIGHT,
                      hp ? C_ACT_BTN : C_DIS_BTN);
    Font_DrawStringCentered(DISP_PREV_X0, (int16_t)DISP_FOOT_Y0, DISP_PREV_X1,
                            LCD_HEIGHT, "< PREV", 1, hp ? C_ACT_FG : C_DIS_FG);

    GUI_FillRectColor(DISP_PREV_X1, DISP_FOOT_Y0, DISP_BACK_X0, LCD_HEIGHT, BLACK);
    GUI_FillRectColor(DISP_BACK_X0, DISP_FOOT_Y0, DISP_BACK_X1, LCD_HEIGHT, C_BACK_BG);
    Font_DrawStringCentered(DISP_BACK_X0, (int16_t)DISP_FOOT_Y0, DISP_BACK_X1,
                            LCD_HEIGHT, "BACK (esc)", 1, C_BACK_FG);
    GUI_FillRectColor(DISP_BACK_X1, DISP_FOOT_Y0, DISP_NEXT_X0, LCD_HEIGHT, BLACK);

    GUI_FillRectColor(DISP_NEXT_X0, DISP_FOOT_Y0, DISP_NEXT_X1, LCD_HEIGHT,
                      hn ? C_ACT_BTN : C_DIS_BTN);
    Font_DrawStringCentered(DISP_NEXT_X0, (int16_t)DISP_FOOT_Y0, DISP_NEXT_X1,
                            LCD_HEIGHT, "NEXT >", 1, hn ? C_ACT_FG : C_DIS_FG);
}

// ---------------------------------------------------------------------------
// Full browse redraw
// ---------------------------------------------------------------------------
static void draw_browse_all(void)
{
    FBWidgetItem_t items[SDBROW_MAX_ITEMS];
    uint8_t n = build_items(items);

    GUI_Clear(BLACK);
    GUI_FillRectColor(0, BR_TITLE_Y0, LCD_WIDTH, BR_TITLE_Y1, C_TITLE_BG);
    Font_DrawStringCentered(0, BR_TITLE_Y0, LCD_WIDTH, BR_TITLE_Y1, "IMAGE", 1, C_TITLE_FG);
    draw_source_selector();
    GUI_FillRectColor(0, BR_SEP_Y0, LCD_WIDTH, BR_SEP_Y1, C_SEP);
    FileBrowserWidget_Draw(&k_list_geom, items, n, s_scroll, s_selected);
    draw_status();
    GUI_FillRectColor(0, BR_STATUS_Y1, LCD_WIDTH, BR_FOOTER_Y0, BLACK);
    draw_browse_footer();
}

// ---------------------------------------------------------------------------
// Enter display mode
// ---------------------------------------------------------------------------
static void enter_display(int8_t idx)
{
    if (idx < 0 || idx >= list_count()) return;

    s_disp_idx = idx;
    s_prev_idx = disp_find_prev(idx);
    s_next_idx = disp_find_next(idx);
    s_mode     = IMG_DISPLAY;

    GUI_Clear(BLACK);

    if (s_source == SOURCE_FLASH) {
        uint8_t slot = s_flash_slots[(uint8_t)idx];
        strncpy(s_disp_name, s_flash_names[(uint8_t)idx], SDBROW_NAME_LEN - 1u);
        s_disp_name[SDBROW_NAME_LEN - 1u] = '\0';
        GUI_FillRectColor(0, 0, LCD_WIDTH, DISP_IMG_H, C_SEL_BG);
        ImgDraw_FromFlash(slot, FLASH_ICON_X, FLASH_ICON_Y);
    } else {
        const SdBrowItem_t *it = SdBrowser_GetItem((uint8_t)idx);
        if (!it || it->type != SDBROW_ITEM_FILE) return;
        strncpy(s_disp_name, it->name, SDBROW_NAME_LEN - 1u);
        s_disp_name[SDBROW_NAME_LEN - 1u] = '\0';

        BmpRc_t rc = bmp_stream(s_disp_name);
        if (rc != BMP_OK) {
            const char *msg;
            switch (rc) {
                case BMP_ERR_OPEN:  msg = "CANNOT OPEN FILE";   break;
                case BMP_ERR_FMT:   msg = "UNSUPPORTED FORMAT"; break;
                case BMP_ERR_LARGE: msg = "IMAGE TOO LARGE";    break;
                default:            msg = "READ ERROR";          break;
            }
            GUI_FillRectColor(0, 0, LCD_WIDTH, DISP_IMG_H, 0x0821u);
            Font_DrawStringCentered(0, DISP_IMG_H/2 - 8, LCD_WIDTH, DISP_IMG_H/2 + 8,
                                    msg, 1, C_STATUS_ERR);
        }
    }

    // Flush stale key state then arm a navigation guard: even if the key is
    // still held after loading, we won't navigate again for 400 ms.
    Navigation_FlushKeyboard();
    s_disp_ready_ms = OS_GetTimeMs() + 400u;

    draw_disp_footer();
}

static void disp_go(int8_t new_idx)
{
    if (new_idx >= 0) enter_display(new_idx);
}

// ---------------------------------------------------------------------------
// Logic: source switch
// ---------------------------------------------------------------------------
static void switch_source(ImgSource_t src)
{
    if (s_source == src) return;
    s_source   = src;
    s_selected = -1;
    s_scroll   = 0;

    FBWidgetItem_t items[SDBROW_MAX_ITEMS];
    uint8_t n = build_items(items);
    draw_source_selector();
    FileBrowserWidget_Draw(&k_list_geom, items, n, s_scroll, s_selected);
    draw_status();
    draw_browse_footer();
}

// ---------------------------------------------------------------------------
// Logic: selection
// ---------------------------------------------------------------------------
static void browse_select(int8_t idx)
{
    int8_t total = list_count();
    if (total == 0) { s_selected = -1; return; }
    if (idx < 0)      idx = (int8_t)(total - 1);
    if (idx >= total) idx = 0;
    s_selected = idx;
    if (s_selected < s_scroll)
        s_scroll = s_selected;
    else if (s_selected >= s_scroll + (int8_t)BR_LIST_ROWS)
        s_scroll = (int8_t)(s_selected - (int8_t)BR_LIST_ROWS + 1);

    FBWidgetItem_t items[SDBROW_MAX_ITEMS];
    uint8_t n = build_items(items);
    FileBrowserWidget_Draw(&k_list_geom, items, n, s_scroll, s_selected);
    draw_status();
    draw_browse_footer();
}

// ---------------------------------------------------------------------------
// Logic: activate
// ---------------------------------------------------------------------------
static void browse_activate(int8_t idx)
{
    if (s_source == SOURCE_FLASH) {
        // Flash items are always files — double-tap or VIEW to display
        if (idx == s_selected) enter_display(idx);
        else                   browse_select(idx);
        return;
    }

    const SdBrowItem_t *it = SdBrowser_GetItem((uint8_t)idx);
    if (!it) return;

    if (it->type == SDBROW_ITEM_PARENT) {
        SdBrowser_GoUp();
        s_selected = -1; s_scroll = 0;
        draw_browse_all();
        return;
    }
    if (it->type == SDBROW_ITEM_DIR) {
        SdBrowser_EnterDir((uint8_t)idx);
        s_selected = -1; s_scroll = 0;
        draw_browse_all();
        return;
    }
    // FILE: first tap selects, second opens
    if (idx == s_selected) enter_display(idx);
    else                   browse_select(idx);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void SceneImage_OnEnter(void)
{
    s_mode          = IMG_BROWSE;
    s_source        = SOURCE_SD;
    s_selected      = -1;
    s_scroll        = 0;
    s_prev_idx      = -1;
    s_next_idx      = -1;
    s_disp_ready_ms = 0u;

    flash_scan();
    SdBrowser_Init(s_bmp_exts, 1u);
    SdBrowser_Mount();
    draw_browse_all();
}

bool SceneImage_OnUpdate(uint32_t now_ms, NavigationEvent_t event)
{
    // ---- DISPLAY mode ------------------------------------------------
    if (s_mode == IMG_DISPLAY) {
        bool nav_ok = (OS_GetTimeMs() >= s_disp_ready_ms);

        if (event == NAVIGATION_TOUCH) {
            int16_t tx, ty;
            Navigation_GetTouchPosition(&tx, &ty);
            if (ty >= (int16_t)DISP_FOOT_Y0) {
                if      (tx < DISP_BACK_X0) { if (nav_ok) disp_go(s_prev_idx); }
                else if (tx < DISP_BACK_X1) { s_mode = IMG_BROWSE; draw_browse_all(); }
                else                         { if (nav_ok) disp_go(s_next_idx); }
            }
            return true;
        }
        switch (event) {
        case NAVIGATION_LEFT:  if (nav_ok) disp_go(s_prev_idx);        return true;
        case NAVIGATION_RIGHT: if (nav_ok) disp_go(s_next_idx);        return true;
        case NAVIGATION_BACK:  s_mode = IMG_BROWSE; draw_browse_all(); return true;
        default:               return true;
        }
    }

    // ---- BROWSE mode -------------------------------------------------
    if (s_source == SOURCE_SD && SdBrowser_Poll(now_ms)) {
        s_selected = -1; s_scroll = 0;
        draw_browse_all();
    }

    if (event == NAVIGATION_TOUCH) {
        int16_t tx, ty;
        Navigation_GetTouchPosition(&tx, &ty);

        // Source selector
        if (ty >= BR_SEL_Y0 && ty < BR_SEL_Y1) {
            if      (tx >= BR_BTN_SD_X0 && tx < BR_BTN_SD_X1) switch_source(SOURCE_SD);
            else if (tx >= BR_BTN_FL_X0 && tx < BR_BTN_FL_X1) switch_source(SOURCE_FLASH);
            return false;
        }

        // File list
        uint8_t vis;
        if (FileBrowserWidget_HitTest(&k_list_geom, tx, ty, &vis)) {
            int8_t idx = s_scroll + (int8_t)vis;
            if (idx < list_count()) browse_activate(idx);
            return false;
        }

        // Footer
        if (ty >= BR_FOOTER_Y0) {
            if (tx < BR_BACK_X1) {
                DemoApp_RequestExit();
            } else if (tx >= BR_VIEW_X0 && selected_is_viewable()) {
                enter_display(s_selected);
            }
            return false;
        }
        return false;
    }

    switch (event) {
    case NAVIGATION_LEFT:
    case NAVIGATION_RIGHT:
        switch_source(s_source == SOURCE_SD ? SOURCE_FLASH : SOURCE_SD);
        break;
    case NAVIGATION_UP:
        if (list_count() > 0) browse_select(s_selected - 1);
        break;
    case NAVIGATION_DOWN:
        if (list_count() > 0) browse_select(s_selected + 1);
        break;
    case NAVIGATION_CONFIRM:
        if (s_selected >= 0) browse_activate(s_selected);
        break;
    default:
        break;
    }

    (void)now_ms;
    return false;
}

void SceneImage_OnExit(void)
{
    SdBrowser_Unmount();
}
