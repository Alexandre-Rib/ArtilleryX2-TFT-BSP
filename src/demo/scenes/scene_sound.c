/**
 * @file    scene_sound.c
 * @brief   Scene: tone-sequence player -- Flash (W25Q64) or .snd files (SD browser)
 * @version 5.0
 * @date    Created: 2026-05-31
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Layout (320x240):
 *    y=  0.. 19 : title bar "SOUND"                  (20 px)
 *    y= 20.. 47 : source selector [SD] [FLASH]        (28 px)
 *    y= 48.. 49 : separator                            ( 2 px)
 *    y= 50..196 : list -- 7 rows x 21 px              (147 px)
 *    y=197..212 : status bar (path / playing name)     (16 px)
 *    y=213      : gap                                  ( 1 px)
 *    y=214..239 : footer [BACK | PLAY/STOP]            (26 px)
 *
 *  SD source uses SdBrowser (sd_browser.c) for all filesystem navigation.
 *  Flash source scans RES_SND_SLOT_COUNT slots in W25Q64.
 */

#include "scene_sound.h"
#include "sd_browser.h"
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
#define SND_LIST_ROWS     7
#define SND_ROW_H        21
#define SND_STATUS_H     16
#define SND_FOOTER_H     26

#define SND_TITLE_Y0      0
#define SND_TITLE_Y1      SND_TITLE_H
#define SND_SEL_Y0        SND_TITLE_Y1
#define SND_SEL_Y1       (SND_SEL_Y0 + SND_SEL_H)
#define SND_SEP_Y0        SND_SEL_Y1
#define SND_SEP_Y1       (SND_SEP_Y0 + SND_SEP_H)
#define SND_LIST_Y0       SND_SEP_Y1
#define SND_LIST_Y1      (SND_LIST_Y0 + SND_LIST_ROWS * SND_ROW_H)
#define SND_STATUS_Y0     SND_LIST_Y1
#define SND_STATUS_Y1    (SND_STATUS_Y0 + SND_STATUS_H)
#define SND_FOOTER_Y0    (LCD_HEIGHT - SND_FOOTER_H)

#define SND_BTN_W        100
#define SND_BTN_H         20
#define SND_BTN_GAP        8
#define SND_BTN_Y0       (SND_SEL_Y0 + (SND_SEL_H - SND_BTN_H) / 2)
#define SND_BTN_Y1       (SND_BTN_Y0 + SND_BTN_H)
#define SND_BTN_SD_X0    ((LCD_WIDTH - 2 * SND_BTN_W - SND_BTN_GAP) / 2)
#define SND_BTN_SD_X1    (SND_BTN_SD_X0 + SND_BTN_W)
#define SND_BTN_FL_X0    (SND_BTN_SD_X1 + SND_BTN_GAP)
#define SND_BTN_FL_X1    (SND_BTN_FL_X0 + SND_BTN_W)

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
#define C_ROW_ODD        0x0821u
#define C_ROW_EVEN       0x0000u
#define C_ROW_FOCUS      0x1842u
#define C_ROW_PLAY       0x0180u
#define C_ROW_FG         0xFFFFu
#define C_ROW_FG_PLAY    0x07E0u
#define C_ROW_FG_PARENT  0x07FFu   // cyan  -- ".." parent entry
#define C_ROW_FG_DIR     0xFD20u   // amber -- directory
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
// Note buffer -- shared between Flash and SD loading
// ---------------------------------------------------------------------------
#define NOTE_BUF_SIZE  256u

static ToneNote_t s_note_buf[NOTE_BUF_SIZE];
static uint16_t   s_note_count;
static char       s_play_name[SDBROW_NAME_LEN];

// ---------------------------------------------------------------------------
// Playback state
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

// Accepted extensions for the SD browser in this scene
static const char *const s_snd_exts[] = {"snd"};

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
    const SdBrowItem_t *item = SdBrowser_GetItem((uint8_t)idx);
    return item ? item->name : "";
}

static bool sd_selected_is_file(void)
{
    if (s_source != SOURCE_SD || s_selected < 0) return false;
    const SdBrowItem_t *item = SdBrowser_GetItem((uint8_t)s_selected);
    return item && item->type == SDBROW_ITEM_FILE;
}

// ---------------------------------------------------------------------------
// Flash: scan all slots, populate only the installed ones
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
// Flash: load notes from a discovered slot into s_note_buf
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
// SD: load selected .snd file into s_note_buf
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
    s_play.notes   = s_note_buf;
    s_play.count   = s_note_count;
    s_play.pos     = 0;
    s_play.active  = true;

    if (s_note_buf[0].freq == 0u) Buzzer_Stop();
    else                          Buzzer_Set(s_note_buf[0].freq, 80u);

    s_play.next_ms = now_ms + s_note_buf[0].dur_ms;
}

static void playback_stop(void)
{
    Buzzer_Stop();
    s_play.active = false;
    s_play.pos    = 0;
}

// ---------------------------------------------------------------------------
// Draw helpers
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
// Draw: list
// ---------------------------------------------------------------------------
static void draw_list_row(int8_t vis_idx)
{
    int8_t  item_idx = s_scroll + vis_idx;
    int8_t  total    = list_count();
    int16_t y0 = (int16_t)(SND_LIST_Y0 + (int16_t)vis_idx * SND_ROW_H);
    int16_t y1 = y0 + SND_ROW_H;

    if (item_idx >= total) {
        uint16_t bg = ((uint8_t)vis_idx & 1u) ? C_ROW_ODD : C_ROW_EVEN;
        GUI_FillRectColor(0,(uint16_t)y0,LCD_WIDTH,(uint16_t)y1,bg);
        return;
    }

    bool is_play = false;
    if (s_play.active) {
        if (s_source == SOURCE_FLASH) {
            is_play = (item_idx == s_selected);
        } else {
            const SdBrowItem_t *it = SdBrowser_GetItem((uint8_t)item_idx);
            is_play = it && it->type == SDBROW_ITEM_FILE &&
                      strcmp(it->name, s_play_name) == 0;
        }
    }
    bool is_foc = (item_idx == s_selected);

    uint16_t bg, fg;
    if (is_play)     { bg = C_ROW_PLAY;  fg = C_ROW_FG_PLAY; }
    else if (is_foc) { bg = C_ROW_FOCUS; fg = C_ROW_FG; }
    else             { bg = ((uint8_t)vis_idx & 1u) ? C_ROW_ODD : C_ROW_EVEN;
                       fg = C_ROW_FG; }

    GUI_FillRectColor(0,(uint16_t)y0,LCD_WIDTH,(uint16_t)y1,bg);

    if (s_source == SOURCE_SD && !is_foc && !is_play) {
        const SdBrowItem_t *it = SdBrowser_GetItem((uint8_t)item_idx);
        if (it) {
            if      (it->type == SDBROW_ITEM_PARENT) { GUI_FillRectColor(0,(uint16_t)y0,3,(uint16_t)y1,C_ROW_FG_PARENT); fg = C_ROW_FG_PARENT; }
            else if (it->type == SDBROW_ITEM_DIR)    { GUI_FillRectColor(0,(uint16_t)y0,3,(uint16_t)y1,C_ROW_FG_DIR);    fg = C_ROW_FG_DIR;    }
        }
    } else if (is_foc || is_play) {
        uint16_t acc = is_play ? C_ROW_FG_PLAY : C_SEP;
        GUI_FillRectColor(0,(uint16_t)y0,3,(uint16_t)y1,acc);
    }

    Font_DrawStringCentered(6, y0, LCD_WIDTH-4, y1, item_name(item_idx), 1, fg);
}

static void draw_list(void)
{
    for (int8_t i = 0; i < (int8_t)SND_LIST_ROWS; i++) draw_list_row(i);
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
static void draw_static_layout(void)
{
    GUI_Clear(BLACK);
    GUI_FillRectColor(0, SND_TITLE_Y0, LCD_WIDTH, SND_TITLE_Y1, C_TITLE_BG);
    Font_DrawStringCentered(0, SND_TITLE_Y0, LCD_WIDTH, SND_TITLE_Y1, "SOUND", 1, C_TITLE_FG);
    draw_source_selector();
    GUI_FillRectColor(0, SND_SEP_Y0, LCD_WIDTH, SND_SEP_Y1, C_SEP);
    draw_list();
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
    draw_list();
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
    draw_source_selector();
    draw_list();
    draw_status();
    draw_footer();
}

// ---------------------------------------------------------------------------
// Logic: activate the focused item
// ---------------------------------------------------------------------------
static void activate_item(int8_t idx, uint32_t now_ms)
{
    if (s_source == SOURCE_SD) {
        const SdBrowItem_t *item = SdBrowser_GetItem((uint8_t)idx);
        if (!item) return;

        if (item->type == SDBROW_ITEM_PARENT) {
            playback_stop();
            SdBrowser_GoUp();
            s_selected = -1; s_scroll = 0;
            draw_list(); draw_status(); draw_footer();
            return;
        }
        if (item->type == SDBROW_ITEM_DIR) {
            playback_stop();
            SdBrowser_EnterDir((uint8_t)idx);
            s_selected = -1; s_scroll = 0;
            draw_list(); draw_status(); draw_footer();
            return;
        }
        // SDBROW_ITEM_FILE falls through to play toggle below
    }

    // Flash or SD FILE: first activate selects; second toggles play/stop
    if (idx == s_selected) {
        if (s_play.active) {
            playback_stop();
            int8_t vis = s_selected - s_scroll;
            if (vis >= 0 && vis < (int8_t)SND_LIST_ROWS) draw_list_row(vis);
            draw_status();
            draw_footer();
        } else {
            bool loaded = (s_source == SOURCE_FLASH) ? flash_load(s_selected)
                                                      : sd_load(s_selected);
            if (!loaded) return;
            strncpy(s_play_name, item_name(s_selected), SDBROW_NAME_LEN - 1u);
            s_play_name[SDBROW_NAME_LEN - 1u] = '\0';
            playback_start(now_ms);
            int8_t vis = s_selected - s_scroll;
            if (vis >= 0 && vis < (int8_t)SND_LIST_ROWS) draw_list_row(vis);
            draw_status();
            draw_footer();
        }
    } else {
        select_item(idx);
    }
}

// ---------------------------------------------------------------------------
// Playback tick
// ---------------------------------------------------------------------------
static void playback_tick(uint32_t now_ms)
{
    if (!s_play.active) return;
    if (now_ms < s_play.next_ms) return;

    s_play.pos++;
    if (s_play.pos >= s_play.count) {
        playback_stop();
        draw_list();
        draw_status();
        draw_footer();
        return;
    }

    const ToneNote_t *n = &s_play.notes[s_play.pos];
    if (n->freq == 0u) Buzzer_Stop();
    else               Buzzer_Set(n->freq, 80u);
    s_play.next_ms = now_ms + n->dur_ms;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void SceneSound_OnEnter(void)
{
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
    draw_static_layout();
}

bool SceneSound_OnUpdate(uint32_t now_ms, NavigationEvent_t event)
{
    playback_tick(now_ms);

    if (event == NAVIGATION_TOUCH) {
        int16_t tx, ty;
        Navigation_GetTouchPosition(&tx, &ty);

        if (ty >= SND_SEL_Y0 && ty < SND_SEL_Y1) {
            if      (tx >= SND_BTN_SD_X0 && tx < SND_BTN_SD_X1) switch_source(SOURCE_SD);
            else if (tx >= SND_BTN_FL_X0 && tx < SND_BTN_FL_X1) switch_source(SOURCE_FLASH);
            return false;
        }

        if (ty >= SND_LIST_Y0 && ty < SND_LIST_Y1) {
            int8_t vis = (int8_t)((ty - SND_LIST_Y0) / SND_ROW_H);
            int8_t idx = s_scroll + vis;
            if (idx < list_count()) activate_item(idx, now_ms);
            return false;
        }

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
}
