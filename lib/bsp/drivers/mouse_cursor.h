/**
 * @file    mouse_cursor.h
 * @brief   Software mouse cursor — 12×12 red/white arrow, save-under via LCD read-back
 * @version 1.0
 * @date    Created: 2026-06-02
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Hotspot at (0,0) — top-left pixel of the cursor bitmap.
 *  Cursor position is clamped so the hotspot stays within [0, LCD_WIDTH-1] × [0, LCD_HEIGHT-1].
 *  Cursor bitmap may extend partially off-screen at the edges (only visible part is drawn).
 *
 *  Usage pattern (called from the main loop):
 *    MouseCursor_Hide()          -- erase cursor, restore background
 *    Navigation_Poll()           -- may call MouseCursor_MoveDelta() internally
 *    scene_on_update()           -- scene may freely redraw the screen
 *    if (Mouse_IsConnected())
 *        MouseCursor_Show()      -- save new background, draw cursor on top
 *
 *  Call MouseCursor_Invalidate() after any full-screen clear so that
 *  Hide() does not restore a stale background.
 */

#ifndef _MOUSE_CURSOR_H_
#define _MOUSE_CURSOR_H_

#include <stdint.h>
#include <stdbool.h>

#define MOUSE_CURSOR_W  12
#define MOUSE_CURSOR_H  12

// Initialise cursor state (call once at startup).
void MouseCursor_Init(void);

// Move cursor by (dx, dy). Hotspot stays within screen bounds.
// Does NOT redraw — draw is done by MouseCursor_Show().
void MouseCursor_MoveDelta(int8_t dx, int8_t dy);

// Jump cursor to absolute screen position (clamped to bounds).
// Does NOT redraw.
void MouseCursor_SetPos(int16_t x, int16_t y);

// Save the background pixels and draw cursor at current position.
// Safe to call when cursor is already visible (no-op in that case).
void MouseCursor_Show(void);

// Restore background pixels (erase cursor from screen).
// Safe to call when cursor is already hidden (no-op in that case).
void MouseCursor_Hide(void);

// Mark cursor as not-drawn without restoring background.
// Call after any full-screen redraw that wiped out the cursor.
void MouseCursor_Invalidate(void);

// Returns true when cursor has been drawn (Show() was called and not yet Hide()'d).
bool MouseCursor_IsVisible(void);

// Get current hotspot position.
void MouseCursor_GetPos(int16_t *x, int16_t *y);

#endif
