/**
 * @file    scene_sound.c
 * @brief   Scene: tone-sequence player — Flash (W25Q64) or .snd files (SD browser)
 * @version 6.0
 * @date    Created: 2026-05-31
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Layout (320x240):
 *    y=  0.. 19 : title bar "SOUND"               (20 px)
 *    y= 20.. 47 : source selector [SD] [FLASH]     (28 px)
 *    y= 48.. 49 : separator                         ( 2 px)
 *    y= 50..175 : file list — 6 rows x 21 px       (126 px)
 *    y=176..199 : volume slider                     (24 px)
 *    y=200..212 : status bar                        (13 px)
 *    y=213      : gap                               ( 1 px)
 *    y=214..239 : footer [BACK | PLAY/STOP]         (26 px)
 *
 *  Volume (0–100) is the global PWM duty-cycle for the buzzer.
 *  It is persisted in Settings (W25Q64 settings sector) and restored on enter.
 *  Touch anywhere on the slider bar sets volume proportionally.
 *
 *  File list rendering is delegated to FileBrowserWidget (ui_filebrowser.h).
 *  SD browsing state is owned by SdBrowser (sd_browser.h).
 *  Flash slot state is owned locally (flash_scan / flash_load).
 */

#include "scene_sound.h"
#include "sd_browser.h"
#include "ui_filebrowser.h"
#include "settings.h"
#include "demo_app.h"
#include "font_embedded.h"
#include "ui_nav.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "buzzer.h"
#include "os_timer.h"
#include "mks_tft28.h"
#include "res_map.h"
#include "flash_map.h"
#include "ff.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
#define SND_TITLE_H      20
#define SND_SEL_H        28
#define SND_SEP_H         2
#define SND_LIST_ROWS     6
#define SND_ROW_H        21
#define SND_VOL_H        24
#define SND_STATUS_H     13
#define SND_FOOTER_H     26

#define SND_TITLE_Y0      0
#define SND_TITLE_Y1      SND_TITLE_H
#define SND_SEL_Y0        SND_TITLE_Y1
#define SND_SEL_Y1       (SND_SEL_Y0  + SND_SEL_H)
#define SND_SEP_Y0        SND_SEL_Y1
#define SND_SEP_Y1       (SND_SEP_Y0  + SND_SEP_H)
#define SND_LIST_Y0       SND_SEP_Y1
#define SND_LIST_Y1      (SND_LIST_Y0 + SND_LIST_ROWS * SND_ROW_H)
#define SND_VOL_Y0        SND_LIST_Y1
#define SND_VOL_Y1       (SND_VOL_Y0  + SND_VOL_H)
#define SND_STATUS_Y0     SND_VOL_Y1
#define SND_STATUS_Y1    (SND_STATUS_Y0 + SND_STATUS_H)
#define SND_FOOTER_Y0    (LCD_HEIGHT - SND_FOOTER_H)

// File-list widget geometry (matches layout above)
static const FBWidgetGeom_t k_list_geom = {
    0, SND_LIST_Y0, LCD_WIDTH, SND_LIST_ROWS, SND_ROW_H
};

// Source selector buttons
#define SND_BTN_W        100
#define SND_BTN_H         20
#define SND_BTN_GAP        8
#define SND_BTN_Y0       (SND_SEL_Y0 + (SND_SEL_H - SND_BTN_H) / 2)
#define SND_BTN_Y1       (SND_BTN_Y0 + SND_BTN_H)
#define SND_BTN_SD_X0    ((LCD_WIDTH - 2 * SND_BTN_W - SND_BTN_GAP) / 2)
#define SND_BTN_SD_X1    (SND_BTN_SD_X0 + SND_BTN_W)
#define SND_BTN_FL_X0    (SND_BTN_SD_X1 + SND_BTN_GAP)
#define SND_BTN_FL_X1    (SND_BTN_FL_X0 + SND_BTN_W)

// Volume slider geometry within SND_VOL zone
#define VOL_LABEL_W      36u
#define VOL_VALUE_W      36u
#define VOL_TRACK_X0     VOL_LABEL_W
#define VOL_TRACK_X1    (LCD_WIDTH - VOL_VALUE_W)
#define VOL_TRACK_W     (VOL_TRACK_X1 - VOL_TRACK_X0)
#define VOL_BAR_H        6u
#define VOL_BAR_Y0       (SND_VOL_Y0 + (SND_VOL_H - VOL_BAR_H) / 2)
#define VOL_BAR_Y1       (VOL_BAR_Y0 + VOL_BAR_H)

// Footer
#define SND_BACK_X0       0
#define SND_BACK_X1     158
#define SND_PLAY_X0     162
#define SND_PLAY_X1     LCD_WIDTH

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------
#define C_TITLE_BG       0x000Fu
#define C_TITLE_FG       0xFFFFu
#define C_SEL_BG         0x0821u
#define C_SEP            0x07FFu
#define C_BTN_ACT_BG     0x000Fu
#define C_BTN_ACT_FG     0xFFE0u
#define C_BTN_ACT_BD     0x07FFu
#define C_BTN_IDL_BG     0x2104u
#define C_BTN_IDL_FG     0x4208u
#define C_STATUS_BG      0x0821u
#define C_STATUS_IDLE    0x4208u
#define C_STATUS_SEL     0x8410u
#define C_STATUS_PLAY    0x07E0u
#define C_STATUS_WARN    0xFD20u
#define C_BACK_BG        0x8800u
#define C_BACK_FG        0xFFFFu
#define C_PLAY_IDL       0x4208u
#define C_PLAY_RDY       0x0400u
#define C_PLAY_ACT       0x8400u
#define C_PLAY_FG        0xFFFFu
#define C_VOL_BG         0x0821u
#define C_VOL_TRACK      0x2104u
#define C_VOL_FILL       0x07FFu   // cyan filled bar
#define C_VOL_LABEL      0x8410u
#define C_VOL_VALUE      0xFFFFu

// ---------------------------------------------------------------------------
// Tone note
// ---------------------------------------------------------------------------
typedef struct {
    uint16_t freq;
    uint16_t dur_ms;
} ToneNote_t;

// ---------------------------------------------------------------------------
// Flash slot discovery
// ---------------------------------------------------------------------------
#define FLASH_NAME_LEN  9u

static char    s_flash_names[RES_SND_SLOT_COUNT][FLASH_NAME_LEN];
static uint8_t s_flash_slots[RES_SND_SLOT_COUNT];
static uint8_t s_flash_count;

// ---------------------------------------------------------------------------
// Note buffer — shared by Flash and SD loading
// ---------------------------------------------------------------------------
#define NOTE_BUF_SIZE  256u

static ToneNote_t s_note_buf[NOTE_BUF_SIZE];
static uint16_t   s_note_count;
static char       s_play_name[SDBROW_NAME_LEN];

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------
typedef struct {
    const ToneNote_t *notes;
    uint16_t          count;
    uint16_t          pos;
    uint32_t          next_ms;
    bool              active;
} Playback_t;

static Playback_t s_play;

// ---------------------------------------------------------------------------
// Scene state
// ---------------------------------------------------------------------------
typedef enum { SOURCE_FLASH, SOURCE_SD } SoundSource_t;

static SoundSource_t s_source;
static int8_t        s_selected;
static int8_t        s_scroll;
static uint8_t       s_volume;   // 0–100, persisted via Settings

static const char *const s_snd_exts[] = {"snd"};

// Forward declarations (draw helpers defined later)
static void draw_status(void);
static void draw_footer(void);

// ---------------------------------------------------------------------------
// Item array builder — feeds the FileBrowserWidget
// ---------------------------------------------------------------------------
static uint8_t build_items(FBWidgetItem_t *items)
{
    uint8_t n;
    if (s_source == SOURCE_FLASH) {
        n = s_flash_count;
        for (uint8_t i = 0; i < n; i++) {
            items[i].type      = FBWI_FILE;
            items[i].name      = s_flash_names[i];
            items[i].is_playing = s_play.active && (i == (uint8_t)s_selected);
        }
    } else {
        n = SdBrowser_GetItemCount();
        for (uint8_t i = 0; i < n; i++) {
            const SdBrowItem_t *it = SdBrowser_GetItem(i);
            items[i].type      = (FBWidgetItemType_t)it->type;
            items[i].name      = it->name;
            items[i].is_playing = s_play.active &&
                                  it->type == SDBROW_ITEM_FILE &&
                                  strcmp(it->name, s_play_name) == 0;
        }
    }
    return n;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static int8_t list_count(void)
{
    return (s_source == SOURCE_FLASH) ? (int8_t)s_flash_count
                                      : (int8_t)SdBrowser_GetItemCount();
}

static const char *item_name(int8_t idx)
{
    if (s_source == SOURCE_FLASH) return s_flash_names[idx];
    const SdBrowItem_t *it = SdBrowser_GetItem((uint8_t)idx);
    return it ? it->name : "";
}

static bool sd_selected_is_file(void)
{
    if (s_source != SOURCE_SD || s_selected < 0) return false;
    const SdBrowItem_t *it = SdBrowser_GetItem((uint8_t)s_selected);
    return it && it->type == SDBROW_ITEM_FILE;
}

// ---------------------------------------------------------------------------
// Flash: scan installed slots
// ---------------------------------------------------------------------------
static void flash_scan(void)
{
    s_flash_count = 0;
    for (uint8_t i = 0; i < RES_SND_SLOT_COUNT && s_flash_count < RES_SND_SLOT_COUNT; i++) {
        char name_buf[9];
        FlashMap_Read(RES_SND_ADDR(i), (uint8_t*)name_buf, 8u);
        name_buf[8] = '\0';
        if ((uint8_t)name_buf[0] == 0xFFu) continue;
        memcpy(s_flash_names[s_flash_count], name_buf, 9u);
        s_flash_slots[s_flash_count] = i;
        s_flash_count++;
    }
}

// ---------------------------------------------------------------------------
// Flash: load notes from slot into s_note_buf
// ---------------------------------------------------------------------------
static bool flash_load(int8_t display_idx)
{
    uint32_t addr   = RES_SND_ADDR(s_flash_slots[display_idx]);
    uint32_t offset = 8u;
    s_note_count = 0;
    while (s_note_count < NOTE_BUF_SIZE) {
        ToneNote_t n;
        FlashMap_Read(addr + offset, (uint8_t*)&n, sizeof(n));
        offset += sizeof(n);
        if (n.freq == 0xFFFFu) break;
        s_note_buf[s_note_count++] = n;
    }
    return s_note_count > 0u;
}

// ---------------------------------------------------------------------------
// SD: load .snd file into s_note_buf
// ---------------------------------------------------------------------------
static bool sd_load(int8_t idx)
{
    char path[SDBROW_PATH_LEN + SDBROW_NAME_LEN];
    if (!SdBrowser_GetFilePath((uint8_t)idx, path, (uint16_t)sizeof(path)))
        return false;

    FIL  fp;
    UINT br;
    if (f_open(&fp, path, FA_READ) != FR_OK) return false;

    s_note_count = 0;
    while (s_note_count < NOTE_BUF_SIZE) {
        ToneNote_t n;
        if (f_read(&fp, &n, sizeof(n), &br) != FR_OK || br != sizeof(n)) break;
        if (n.freq == 0xFFFFu) break;
        s_note_buf[s_note_count++] = n;
    }
    f_close(&fp);
    return s_note_count > 0u;
}

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------
static void playback_start(uint32_t now_ms)
{
    s_play.notes  = s_note_buf;
    s_play.count  = s_note_count;
    s_play.pos    = 0;
    s_play.active = true;

    if (s_note_buf[0].freq == 0u) Buzzer_Stop();
    else                          Buzzer_Set(s_note_buf[0].freq, s_volume);
    s_play.next_ms = now_ms + s_note_buf[0].dur_ms;
}

static void playback_stop(void)
{
    Buzzer_Stop();
    s_play.active = false;
    s_play.pos    = 0;
}

static void playback_tick(uint32_t now_ms)
{
    if (!s_play.active || now_ms < s_play.next_ms) return;
    s_play.pos++;
    if (s_play.pos >= s_play.count) {
        playback_stop();
        FBWidgetItem_t items[SDBROW_MAX_ITEMS];
        uint8_t n = build_items(items);
        FileBrowserWidget_Draw(&k_list_geom, items, n, s_scroll, s_selected);
        draw_status();
        draw_footer();
        return;
    }
    const ToneNote_t *note = &s_play.notes[s_play.pos];
    if (note->freq == 0u) Buzzer_Stop();
    else                  Buzzer_Set(note->freq, s_volume);
    s_play.next_ms = now_ms + note->dur_ms;
}

// ---------------------------------------------------------------------------
// Volume: save to flash (preserves existing calibration)
// ---------------------------------------------------------------------------
static void volume_save(void)
{
    Settings_t cfg;
    if (!Settings_Load(&cfg)) Settings_GetDefault(&cfg);
    cfg.sound_volume = s_volume;
    Settings_Save(&cfg);
}

// ---------------------------------------------------------------------------
// Draw helpers — icons
// ---------------------------------------------------------------------------
static void draw_back_arrow(int16_t cx, int16_t cy)
{
    GUI_SetColor(C_BACK_FG);
    GUI_DrawLine((uint16_t)(cx+7),(uint16_t)(cy-7),(uint16_t)(cx-4),(uint16_t)cy);
    GUI_DrawLine((uint16_t)(cx-4),(uint16_t)cy,    (uint16_t)(cx+7),(uint16_t)(cy+7));
    GUI_DrawLine((uint16_t)(cx+8),(uint16_t)(cy-7),(uint16_t)(cx-3),(uint16_t)cy);
    GUI_DrawLine((uint16_t)(cx-3),(uint16_t)cy,    (uint16_t)(cx+8),(uint16_t)(cy+7));
}

static void draw_play_icon(int16_t cx, int16_t cy, uint16_t color)
{
    GUI_SetColor(color);
    GUI_DrawLine((uint16_t)cx,(uint16_t)(cy-5),(uint16_t)cx,(uint16_t)(cy+5));
    GUI_DrawLine((uint16_t)cx,(uint16_t)(cy-5),(uint16_t)(cx+8),(uint16_t)cy);
    GUI_DrawLine((uint16_t)cx,(uint16_t)(cy+5),(uint16_t)(cx+8),(uint16_t)cy);
}

static void draw_stop_icon(int16_t cx, int16_t cy, uint16_t color)
{
    GUI_FillRectColor((uint16_t)(cx-4),(uint16_t)(cy-4),(uint16_t)(cx+5),(uint16_t)(cy+5),color);
}

// ---------------------------------------------------------------------------
// Draw: source selector
// ---------------------------------------------------------------------------
static void draw_source_btn(int16_t x0, int16_t x1, const char *label, bool active)
{
    uint16_t bg = active ? C_BTN_ACT_BG : C_BTN_IDL_BG;
    uint16_t fg = active ? C_BTN_ACT_FG : C_BTN_IDL_FG;
    GUI_FillRectColor((uint16_t)x0, SND_BTN_Y0, (uint16_t)x1, SND_BTN_Y1, bg);
    if (active) {
        GUI_FillRectColor((uint16_t)x0,    SND_BTN_Y0,    (uint16_t)x1,     SND_BTN_Y0+1, C_BTN_ACT_BD);
        GUI_FillRectColor((uint16_t)x0,    SND_BTN_Y1-1,  (uint16_t)x1,     SND_BTN_Y1,   C_BTN_ACT_BD);
        GUI_FillRectColor((uint16_t)x0,    SND_BTN_Y0,    (uint16_t)(x0+1), SND_BTN_Y1,   C_BTN_ACT_BD);
        GUI_FillRectColor((uint16_t)(x1-1),SND_BTN_Y0,    (uint16_t)x1,     SND_BTN_Y1,   C_BTN_ACT_BD);
    }
    Font_DrawStringCentered(x0, SND_BTN_Y0, x1, SND_BTN_Y1, label, 1, fg);
}

static void draw_source_selector(void)
{
    GUI_FillRectColor(0, SND_SEL_Y0, LCD_WIDTH, SND_SEL_Y1, C_SEL_BG);
    draw_source_btn(SND_BTN_SD_X0, SND_BTN_SD_X1, "SD",    s_source == SOURCE_SD);
    draw_source_btn(SND_BTN_FL_X0, SND_BTN_FL_X1, "FLASH", s_source == SOURCE_FLASH);
}

// ---------------------------------------------------------------------------
// Draw: volume slider
// ---------------------------------------------------------------------------
static void draw_volume(void)
{
    GUI_FillRectColor(0, SND_VOL_Y0, LCD_WIDTH, SND_VOL_Y1, C_VOL_BG);

    // "VOL" label
    Font_DrawStringCentered(0, SND_VOL_Y0, VOL_LABEL_W, SND_VOL_Y1, "VOL", 1, C_VOL_LABEL);

    // Track background
    GUI_FillRectColor(VOL_TRACK_X0, VOL_BAR_Y0, VOL_TRACK_X1, VOL_BAR_Y1, C_VOL_TRACK);

    // Filled bar
    uint16_t filled = (uint16_t)((uint32_t)s_volume * VOL_TRACK_W / 100u);
    if (filled > 0u)
        GUI_FillRectColor(VOL_TRACK_X0, VOL_BAR_Y0,
                          VOL_TRACK_X0 + filled, VOL_BAR_Y1, C_VOL_FILL);

    // Percentage value "  0" .. "100"
    char val[5];
    if (s_volume == 100u) {
        val[0]='1'; val[1]='0'; val[2]='0'; val[3]='%'; val[4]='\0';
    } else {
        val[0] = '0' + s_volume / 10u;
        val[1] = '0' + s_volume % 10u;
        val[2] = '%'; val[3] = '\0';
    }
    Font_DrawStringCentered(VOL_TRACK_X1, SND_VOL_Y0, LCD_WIDTH, SND_VOL_Y1, val, 1, C_VOL_VALUE);
}

// ---------------------------------------------------------------------------
// Draw: status bar
// ---------------------------------------------------------------------------
static void draw_status(void)
{
    GUI_FillRectColor(0, SND_STATUS_Y0, LCD_WIDTH, SND_STATUS_Y1, C_STATUS_BG);

    const char *msg;
    uint16_t    col;

    if (s_play.active) {
        msg = s_play_name;
        col = C_STATUS_PLAY;
    } else if (s_source == SOURCE_FLASH) {
        if (s_flash_count == 0)    { msg = "NO SOUNDS IN FLASH"; col = C_STATUS_IDLE; }
        else if (s_selected >= 0)  { msg = item_name(s_selected); col = C_STATUS_SEL;  }
        else                       { msg = "SELECT + ENTER TO PLAY"; col = C_STATUS_IDLE; }
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
            msg = dbg;
            col = (SdBrowser_GetLastError() == 0) ? C_STATUS_IDLE : C_STATUS_WARN;
        }
    }

    Font_DrawStringCentered(0, SND_STATUS_Y0, LCD_WIDTH, SND_STATUS_Y1, msg, 1, col);
}

// ---------------------------------------------------------------------------
// Draw: footer
// ---------------------------------------------------------------------------
static void draw_footer(void)
{
    int16_t cy = (int16_t)(SND_FOOTER_Y0 + SND_FOOTER_H / 2);

    GUI_FillRectColor(SND_BACK_X0, SND_FOOTER_Y0, SND_BACK_X1, LCD_HEIGHT, C_BACK_BG);
    draw_back_arrow(24, cy);
    Font_DrawStringCentered(48, SND_FOOTER_Y0, SND_BACK_X1, LCD_HEIGHT, "BACK", 1, C_BACK_FG);
    GUI_FillRectColor(SND_BACK_X1, SND_FOOTER_Y0, SND_PLAY_X0, LCD_HEIGHT, BLACK);

    bool can_play = (s_source == SOURCE_FLASH && s_selected >= 0) || sd_selected_is_file();
    uint16_t    play_bg;
    const char *play_lbl;
    if      (s_play.active) { play_bg = C_PLAY_ACT; play_lbl = "STOP"; }
    else if (can_play)      { play_bg = C_PLAY_RDY; play_lbl = "PLAY"; }
    else                    { play_bg = C_PLAY_IDL; play_lbl = "PLAY"; }

    GUI_FillRectColor(SND_PLAY_X0, SND_FOOTER_Y0, SND_PLAY_X1, LCD_HEIGHT, play_bg);
    int16_t ix = (int16_t)(SND_PLAY_X0 + 16);
    if (s_play.active) draw_stop_icon(ix, cy, C_PLAY_FG);
    else               draw_play_icon(ix, cy, C_PLAY_FG);
    Font_DrawStringCentered((int16_t)(SND_PLAY_X0+32), SND_FOOTER_Y0,
                            SND_PLAY_X1, LCD_HEIGHT, play_lbl, 1, C_PLAY_FG);
}

// ---------------------------------------------------------------------------
// Full layout
// ---------------------------------------------------------------------------
static void draw_all(void)
{
    FBWidgetItem_t items[SDBROW_MAX_ITEMS];
    uint8_t n = build_items(items);

    GUI_Clear(BLACK);
    GUI_FillRectColor(0, SND_TITLE_Y0, LCD_WIDTH, SND_TITLE_Y1, C_TITLE_BG);
    Font_DrawStringCentered(0, SND_TITLE_Y0, LCD_WIDTH, SND_TITLE_Y1, "SOUND", 1, C_TITLE_FG);
    draw_source_selector();
    GUI_FillRectColor(0, SND_SEP_Y0, LCD_WIDTH, SND_SEP_Y1, C_SEP);
    FileBrowserWidget_Draw(&k_list_geom, items, n, s_scroll, s_selected);
    draw_volume();
    draw_status();
    GUI_FillRectColor(0, SND_STATUS_Y1, LCD_WIDTH, SND_FOOTER_Y0, BLACK);
    draw_footer();
}

// ---------------------------------------------------------------------------
// Logic: move selection
// ---------------------------------------------------------------------------
static void select_item(int8_t idx)
{
    int8_t total = list_count();
    if (total == 0) { s_selected = -1; return; }
    if (idx < 0)      idx = (int8_t)(total - 1);
    if (idx >= total) idx = 0;
    s_selected = idx;
    if (s_selected < s_scroll)
        s_scroll = s_selected;
    else if (s_selected >= s_scroll + (int8_t)SND_LIST_ROWS)
        s_scroll = (int8_t)(s_selected - (int8_t)SND_LIST_ROWS + 1);

    FBWidgetItem_t items[SDBROW_MAX_ITEMS];
    uint8_t n = build_items(items);
    FileBrowserWidget_Draw(&k_list_geom, items, n, s_scroll, s_selected);
    draw_status();
    draw_footer();
}

// ---------------------------------------------------------------------------
// Logic: switch source
// ---------------------------------------------------------------------------
static void switch_source(SoundSource_t src)
{
    if (s_source == src) return;
    playback_stop();
    s_source   = src;
    s_selected = -1;
    s_scroll   = 0;

    FBWidgetItem_t items[SDBROW_MAX_ITEMS];
    uint8_t n = build_items(items);
    draw_source_selector();
    FileBrowserWidget_Draw(&k_list_geom, items, n, s_scroll, s_selected);
    draw_status();
    draw_footer();
}

// ---------------------------------------------------------------------------
// Logic: activate focused item (navigate or play)
// ---------------------------------------------------------------------------
static void activate_item(int8_t idx, uint32_t now_ms)
{
    if (s_source == SOURCE_SD) {
        const SdBrowItem_t *it = SdBrowser_GetItem((uint8_t)idx);
        if (!it) return;

        if (it->type == SDBROW_ITEM_PARENT) {
            playback_stop();
            SdBrowser_GoUp();
            s_selected = -1; s_scroll = 0;
            FBWidgetItem_t items[SDBROW_MAX_ITEMS];
            uint8_t n = build_items(items);
            FileBrowserWidget_Draw(&k_list_geom, items, n, s_scroll, s_selected);
            draw_status(); draw_footer();
            return;
        }
        if (it->type == SDBROW_ITEM_DIR) {
            playback_stop();
            SdBrowser_EnterDir((uint8_t)idx);
            s_selected = -1; s_scroll = 0;
            FBWidgetItem_t items[SDBROW_MAX_ITEMS];
            uint8_t n = build_items(items);
            FileBrowserWidget_Draw(&k_list_geom, items, n, s_scroll, s_selected);
            draw_status(); draw_footer();
            return;
        }
        // SDBROW_ITEM_FILE falls through
    }

    if (idx == s_selected) {
        if (s_play.active) {
            playback_stop();
            FBWidgetItem_t items[SDBROW_MAX_ITEMS];
            uint8_t n = build_items(items);
            int8_t vis = s_selected - s_scroll;
            if (vis >= 0 && vis < (int8_t)SND_LIST_ROWS)
                FileBrowserWidget_DrawRow(&k_list_geom, items, n, (uint8_t)vis, s_scroll, s_selected);
            draw_status(); draw_footer();
        } else {
            bool loaded = (s_source == SOURCE_FLASH) ? flash_load(s_selected)
                                                      : sd_load(s_selected);
            if (!loaded) return;
            strncpy(s_play_name, item_name(s_selected), SDBROW_NAME_LEN - 1u);
            s_play_name[SDBROW_NAME_LEN - 1u] = '\0';
            playback_start(now_ms);
            FBWidgetItem_t items[SDBROW_MAX_ITEMS];
            uint8_t n = build_items(items);
            int8_t vis = s_selected - s_scroll;
            if (vis >= 0 && vis < (int8_t)SND_LIST_ROWS)
                FileBrowserWidget_DrawRow(&k_list_geom, items, n, (uint8_t)vis, s_scroll, s_selected);
            draw_status(); draw_footer();
        }
    } else {
        select_item(idx);
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void SceneSound_OnEnter(void)
{
    // Restore persistent volume
    Settings_t cfg;
    if (Settings_Load(&cfg) && cfg.sound_volume <= 100u)
        s_volume = cfg.sound_volume;
    else
        s_volume = 40u;

    s_source      = SOURCE_FLASH;
    s_selected    = -1;
    s_scroll      = 0;
    s_flash_count = 0;
    s_note_count  = 0;
    s_play_name[0] = '\0';
    memset(&s_play, 0, sizeof(s_play));

    flash_scan();
    SdBrowser_Init(s_snd_exts, 1u);
    SdBrowser_Mount();
    draw_all();
}

bool SceneSound_OnUpdate(uint32_t now_ms, NavigationEvent_t event)
{
    playback_tick(now_ms);

    // Hot-plug SD detection (throttled to 500 ms inside SdBrowser_Poll)
    if (s_source == SOURCE_SD && SdBrowser_Poll(now_ms)) {
        if (s_play.active) playback_stop();
        s_selected = -1;
        s_scroll   = 0;
        FBWidgetItem_t items[SDBROW_MAX_ITEMS];
        uint8_t n = build_items(items);
        FileBrowserWidget_Draw(&k_list_geom, items, n, s_scroll, s_selected);
        draw_status(); draw_footer();
    }

    if (event == NAVIGATION_TOUCH) {
        int16_t tx, ty;
        Navigation_GetTouchPosition(&tx, &ty);

        // Source selector
        if (ty >= SND_SEL_Y0 && ty < SND_SEL_Y1) {
            if      (tx >= SND_BTN_SD_X0 && tx < SND_BTN_SD_X1) switch_source(SOURCE_SD);
            else if (tx >= SND_BTN_FL_X0 && tx < SND_BTN_FL_X1) switch_source(SOURCE_FLASH);
            return false;
        }

        // File list
        {
            uint8_t vis;
            if (FileBrowserWidget_HitTest(&k_list_geom, tx, ty, &vis)) {
                int8_t idx = s_scroll + (int8_t)vis;
                if (idx < list_count()) activate_item(idx, now_ms);
                return false;
            }
        }

        // Volume slider — update RAM immediately, save deferred to OnExit
        if (ty >= SND_VOL_Y0 && ty < SND_VOL_Y1) {
            if (tx >= (int16_t)VOL_TRACK_X0 && tx < (int16_t)VOL_TRACK_X1) {
                uint32_t v = (uint32_t)(tx - (int16_t)VOL_TRACK_X0) * 100u / VOL_TRACK_W;
                s_volume = (uint8_t)(v > 100u ? 100u : v);
                draw_volume();
            }
            return false;
        }

        // Footer
        if (ty >= SND_FOOTER_Y0) {
            if (tx < SND_BACK_X1) {
                DemoApp_RequestExit();
            } else if (tx >= SND_PLAY_X0) {
                if (s_selected >= 0) activate_item(s_selected, now_ms);
            }
            return false;
        }

        return false;
    }

    switch (event) {
    case NAVIGATION_LEFT:
    case NAVIGATION_RIGHT:
        switch_source(s_source == SOURCE_FLASH ? SOURCE_SD : SOURCE_FLASH);
        break;
    case NAVIGATION_UP:
        if (list_count() > 0) select_item(s_selected - 1);
        break;
    case NAVIGATION_DOWN:
        if (list_count() > 0) select_item(s_selected + 1);
        break;
    case NAVIGATION_CONFIRM:
        if (s_selected >= 0) activate_item(s_selected, now_ms);
        break;
    default:
        break;
    }

    return false;
}

void SceneSound_OnExit(void)
{
    playback_stop();
    SdBrowser_Unmount();
    volume_save();   // single flash sector erase on exit, never during interaction
}
