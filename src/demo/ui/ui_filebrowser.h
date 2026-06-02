/**
 * @file    ui_filebrowser.h
 * @brief   Generic file-list rendering widget (no data logic, no FatFS dependency)
 * @version 1.0
 * @date    Created: 2026-05-31
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  This widget renders a scrollable file list inside a caller-defined rectangle.
 *  The caller builds an FBWidgetItem_t array from any source (SdBrowser, Flash
 *  slots, etc.) and passes it to the draw functions.
 *
 *  Adding extra buttons to a menu is purely a scene-level concern and does not
 *  touch this widget.
 *
 *  Typical usage in a scene:
 *
 *    static const FBWidgetGeom_t list_geom = {0, 50, 320, 6, 21};
 *
 *    FBWidgetItem_t items[SDBROW_MAX_ITEMS];
 *    uint8_t        count = 0;
 *    // ... fill items from SdBrowser or flash list ...
 *
 *    FileBrowserWidget_Draw(&list_geom, items, count, scroll, selected);
 *
 *    // Hit-test on touch:
 *    uint8_t vis;
 *    if (FileBrowserWidget_HitTest(&list_geom, tx, ty, &vis)) {
 *        uint8_t idx = scroll + vis;
 *        // activate items[idx]
 *    }
 */

#ifndef _UI_FILEBROWSER_H_
#define _UI_FILEBROWSER_H_

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Item descriptor — filled by the scene, consumed by the widget
// ---------------------------------------------------------------------------

typedef enum {
    FBWI_PARENT = 0,   ///< ".." go-up entry
    FBWI_DIR    = 1,   ///< Sub-directory  (matches SdBrowItemType_t values)
    FBWI_FILE   = 2,   ///< Regular file
} FBWidgetItemType_t;

typedef struct {
    FBWidgetItemType_t type;
    const char        *name;       ///< Pointer valid for the duration of the draw call
    bool               is_playing; ///< Highlight this row as the active/playing item
} FBWidgetItem_t;

// ---------------------------------------------------------------------------
// Layout descriptor
// ---------------------------------------------------------------------------

typedef struct {
    uint16_t x0, y0;   ///< Top-left corner of the list area
    uint16_t w;         ///< Width in pixels (usually LCD_WIDTH)
    uint8_t  row_count; ///< Number of visible rows
    uint8_t  row_h;     ///< Height of each row in pixels
} FBWidgetGeom_t;

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

/**
 * @brief  Draw all visible rows of the file list.
 *
 * Rows outside the item range are drawn as empty (alternating background).
 *
 * @param g         Layout descriptor.
 * @param items     Item array (length = total logical count, not just visible).
 * @param count     Total number of items in the logical list.
 * @param scroll    Index of the first visible item (scroll offset).
 * @param selected  Logically selected item index, or -1 for none.
 */
void FileBrowserWidget_Draw(const FBWidgetGeom_t *g,
                            const FBWidgetItem_t *items, uint8_t count,
                            int8_t scroll, int8_t selected);

/**
 * @brief  Draw a single visible row (vis_idx = 0 .. row_count-1).
 *
 * Use this for partial redraws (e.g. after selection change).
 */
void FileBrowserWidget_DrawRow(const FBWidgetGeom_t *g,
                               const FBWidgetItem_t *items, uint8_t count,
                               uint8_t vis_idx, int8_t scroll, int8_t selected);

/**
 * @brief  Test whether a touch point falls within the list area.
 *
 * @param[in]  g            Layout descriptor.
 * @param[in]  tx, ty       Touch coordinates in screen pixels.
 * @param[out] out_vis_idx  Visual row index (0 .. row_count-1) that was hit.
 * @return true if the touch is inside the list area, false otherwise.
 */
bool FileBrowserWidget_HitTest(const FBWidgetGeom_t *g,
                               int16_t tx, int16_t ty,
                               uint8_t *out_vis_idx);

#endif /* _UI_FILEBROWSER_H_ */
