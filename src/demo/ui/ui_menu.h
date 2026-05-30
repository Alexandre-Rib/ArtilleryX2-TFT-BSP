/**
 * @file    ui_menu.h
 * @brief   Menu widget — grid navigation by keyboard and touch
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Usage pattern:
 *    1. Declare a MenuItem_t array (geometry, action callback, optional draw function).
 *    2. Wrap in a Menu_t (item count, grid columns, initial focus, optional parent).
 *    3. Call Menu_Draw() to render the initial layout.
 *    4. Each main-loop iteration: pass Navigation_Poll() result to Menu_HandleEvent()
 *       and react to the returned MenuResult_t.
 *
 *  Keyboard navigation:
 *    LEFT / RIGHT / UP / DOWN   — move focus; wraps within row / column
 *    NAVIGATION_CONFIRM (Enter) — activates the focused item; if action == NULL
 *                                 returns MENU_RESULT_BACK
 *    NAVIGATION_BACK   (Escape) — returns MENU_RESULT_BACK without change
 *
 *  Touch navigation:
 *    Tap on a button            — focus + activate in one gesture
 *
 *  Parent / back chain:
 *    Each Menu_t carries a *parent pointer.  When MENU_RESULT_BACK is returned,
 *    the caller navigates to the parent menu or exits the current context.
 *    A NULL parent means root — MENU_RESULT_BACK propagates to the caller.
 */

#ifndef _UI_MENU_H_
#define _UI_MENU_H_

#include "ui_button.h"
#include "ui_nav.h"
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Return value of Menu_HandleEvent()
// ---------------------------------------------------------------------------

/**
 * @brief  Result returned by Menu_HandleEvent() after processing one event.
 */
typedef enum {
    MENU_RESULT_NONE   = 0, ///< No meaningful change.
    MENU_RESULT_FOCUS,      ///< Focus moved to a different item.
    MENU_RESULT_ACTION,     ///< An item's action callback was invoked.
    MENU_RESULT_BACK,       ///< NAVIGATION_BACK pressed, or Enter on a no-action item.
} MenuResult_t;

// ---------------------------------------------------------------------------
// Menu item
// ---------------------------------------------------------------------------

/** @brief  Callback invoked when an item is activated.  NULL = "back / no-op". */
typedef void (*MenuAction)(void);

/** @brief  Optional custom draw function for a menu item.  NULL = Button_Draw(). */
typedef void (*MenuDrawFn)(bool focused);

/**
 * @brief  One item inside a Menu_t.
 */
typedef struct {
    Button_t   button;  ///< Geometry, label, and visual state.
    MenuAction action;  ///< Called on activation; NULL makes the item a back trigger.
    MenuDrawFn draw_fn; ///< Custom renderer; NULL falls back to Button_Draw().
} MenuItem_t;

// ---------------------------------------------------------------------------
// Menu descriptor
// ---------------------------------------------------------------------------

/**
 * @brief  Menu layout and navigation state.
 */
typedef struct Menu {
    MenuItem_t  *items;   ///< Array of menu items.
    uint8_t      count;   ///< Number of items in the array.
    uint8_t      cols;    ///< Grid columns (0 = same as count, i.e. single row).
    uint8_t      focused; ///< Index of the currently focused item.
    struct Menu *parent;  ///< Menu to return to on NAVIGATION_BACK; NULL = root.
} Menu_t;

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

/**
 * @brief  Draw all items of a menu in their current state.
 *
 * Call once after building the Menu_t to render the initial layout, and again
 * when returning from a child menu to restore the visible state.
 *
 * @param[in] menu  Menu to render.
 */
void Menu_Draw(Menu_t *menu);

/**
 * @brief  Move keyboard focus to a specific item and redraw affected buttons.
 *
 * Clears BUTTON_FOCUSED on the previously focused item, then sets it on
 * @p index.  Disabled items cannot receive focus — the call is silently
 * ignored for them.
 *
 * @param[in] menu   Menu to update.
 * @param[in] index  Zero-based slot index to focus (ignored if >= menu->count).
 */
void Menu_Focus(Menu_t *menu, uint8_t index);

/**
 * @brief  Process one navigation event and update the menu accordingly.
 *
 * - Directional events move focus within the grid (wrapping, skipping disabled).
 * - NAVIGATION_CONFIRM activates the focused item (calls its @c action callback).
 *   If the focused item has no action, returns MENU_RESULT_BACK.
 * - NAVIGATION_TOUCH hit-tests all items and activates the tapped one.
 * - NAVIGATION_BACK returns MENU_RESULT_BACK without changing focus.
 *
 * The caller is responsible for reacting to MENU_RESULT_BACK
 * (navigate to menu->parent, or exit the current scene).
 *
 * @param[in] menu   Menu to update.
 * @param[in] event  Navigation event from Navigation_Poll().
 * @return Processing result (see MenuResult_t).
 */
MenuResult_t Menu_HandleEvent(Menu_t *menu, NavigationEvent_t event);

#endif
