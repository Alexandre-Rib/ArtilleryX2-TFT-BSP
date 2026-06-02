/**
 * @file    ui_filebrowser.c
 * @brief   Generic file-list rendering widget
 * @version 1.0
 * @date    Created: 2026-05-31
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#include "ui_filebrowser.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "font_embedded.h"

// ---------------------------------------------------------------------------
// Visual style constants
// ---------------------------------------------------------------------------
#define C_ROW_ODD       0x0821u
#define C_ROW_EVEN      0x0000u
#define C_ROW_FOCUS     0x1842u
#define C_ROW_PLAY      0x0180u
#define C_ROW_FG        0xFFFFu
#define C_ROW_FG_PLAY   0x07E0u
#define C_ROW_FG_PARENT 0x07FFu   // cyan  — ".." entry
#define C_ROW_FG_DIR    0xFD20u   // amber — directory
#define C_ACCENT_SEL    0x07FFu   // cyan accent bar when selected
#define C_ACCENT_PLAY   0x07E0u   // green accent bar when playing

// ---------------------------------------------------------------------------
// Internal: draw one row given its logical item index
// ---------------------------------------------------------------------------
static void draw_row(const FBWidgetGeom_t *g,
                     const FBWidgetItem_t *items, uint8_t count,
                     uint8_t vis_idx, int8_t scroll, int8_t selected)
{
    int8_t   item_idx = (int8_t)scroll + (int8_t)vis_idx;
    uint16_t y0 = (uint16_t)(g->y0 + (uint16_t)vis_idx * g->row_h);
    uint16_t y1 = y0 + g->row_h;

    // Empty row
    if (item_idx < 0 || (uint8_t)item_idx >= count) {
        uint16_t bg = (vis_idx & 1u) ? C_ROW_ODD : C_ROW_EVEN;
        GUI_FillRectColor(g->x0, y0, g->x0 + g->w, y1, bg);
        return;
    }

    const FBWidgetItem_t *it  = &items[(uint8_t)item_idx];
    bool is_play = it->is_playing;
    bool is_foc  = (item_idx == selected);

    uint16_t bg, fg;
    if (is_play)     { bg = C_ROW_PLAY;  fg = C_ROW_FG_PLAY; }
    else if (is_foc) { bg = C_ROW_FOCUS; fg = C_ROW_FG; }
    else             { bg = (vis_idx & 1u) ? C_ROW_ODD : C_ROW_EVEN;
                       fg = C_ROW_FG; }

    GUI_FillRectColor(g->x0, y0, g->x0 + g->w, y1, bg);

    // 3-px left accent bar + color override based on item type
    if (!is_foc && !is_play) {
        if      (it->type == FBWI_PARENT) { GUI_FillRectColor(g->x0, y0, g->x0+3, y1, C_ROW_FG_PARENT); fg = C_ROW_FG_PARENT; }
        else if (it->type == FBWI_DIR)    { GUI_FillRectColor(g->x0, y0, g->x0+3, y1, C_ROW_FG_DIR);    fg = C_ROW_FG_DIR;    }
    } else {
        uint16_t acc = is_play ? C_ACCENT_PLAY : C_ACCENT_SEL;
        GUI_FillRectColor(g->x0, y0, g->x0+3, y1, acc);
    }

    Font_DrawStringCentered((int16_t)(g->x0 + 6), (int16_t)y0,
                            (int16_t)(g->x0 + g->w - 4), (int16_t)y1,
                            it->name, 1, fg);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void FileBrowserWidget_Draw(const FBWidgetGeom_t *g,
                            const FBWidgetItem_t *items, uint8_t count,
                            int8_t scroll, int8_t selected)
{
    for (uint8_t i = 0; i < g->row_count; i++)
        draw_row(g, items, count, i, scroll, selected);
}

void FileBrowserWidget_DrawRow(const FBWidgetGeom_t *g,
                               const FBWidgetItem_t *items, uint8_t count,
                               uint8_t vis_idx, int8_t scroll, int8_t selected)
{
    if (vis_idx >= g->row_count) return;
    draw_row(g, items, count, vis_idx, scroll, selected);
}

bool FileBrowserWidget_HitTest(const FBWidgetGeom_t *g,
                               int16_t tx, int16_t ty,
                               uint8_t *out_vis_idx)
{
    if (tx < (int16_t)g->x0 || tx >= (int16_t)(g->x0 + g->w)) return false;
    if (ty < (int16_t)g->y0 || ty >= (int16_t)(g->y0 + g->row_count * g->row_h)) return false;
    *out_vis_idx = (uint8_t)((ty - (int16_t)g->y0) / g->row_h);
    return true;
}
