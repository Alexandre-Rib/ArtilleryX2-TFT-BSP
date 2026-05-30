/**
 * @file    ui_menu.c
 * @brief   Menu widget — grid navigation by keyboard and touch
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#include "ui_menu.h"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/**
 * @brief  Render one menu item using its custom draw function or Button_Draw().
 *
 * @param[in] menu   Menu that contains the item.
 * @param[in] index  Index of the item to render.
 */
static void render_item(Menu_t *menu, uint8_t index)
{
    MenuItem_t *item = &menu->items[index];
    if (item->draw_fn)
        item->draw_fn(index == menu->focused);
    else
        Button_Draw(&item->button);
}

/**
 * @brief  Compute the next focus index in a given navigation direction.
 *
 * Wraps within the grid row (LEFT/RIGHT) or column (UP/DOWN).
 * Skips items that have no action, no draw function, and are BUTTON_DISABLED
 * (truly inert decoration items).
 *
 * @param[in] menu       Menu to navigate.
 * @param[in] current    Index of the currently focused item.
 * @param[in] direction  NAVIGATION_LEFT / RIGHT / UP / DOWN.
 * @return Index of the next item to focus (equal to @p current if no move possible).
 */
static uint8_t compute_next_focus(Menu_t *menu, uint8_t current,
                                   NavigationEvent_t direction)
{
    uint8_t columns = (menu->cols > 0) ? menu->cols : menu->count;
    uint8_t rows    = (uint8_t)((menu->count + columns - 1) / columns);
    uint8_t col     = current % columns;
    uint8_t row     = current / columns;

    switch (direction) {
        case NAVIGATION_RIGHT: col = (uint8_t)((col + 1)           % columns); break;
        case NAVIGATION_LEFT:  col = (uint8_t)((col + columns - 1) % columns); break;
        case NAVIGATION_DOWN:  row = (uint8_t)((row + 1)           % rows);    break;
        case NAVIGATION_UP:    row = (uint8_t)((row + rows - 1)    % rows);    break;
        default:               return current;
    }

    uint8_t next = (uint8_t)(row * columns + col);
    if (next >= menu->count) return current;

    // Skip truly inert items (no action, no custom draw, explicitly disabled)
    if (!menu->items[next].action &&
        !menu->items[next].draw_fn &&
        menu->items[next].button.state == BUTTON_DISABLED)
        return current;

    return next;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Menu_Draw(Menu_t *menu)
{
    for (uint8_t i = 0; i < menu->count; i++)
        render_item(menu, i);
}

void Menu_Focus(Menu_t *menu, uint8_t index)
{
    if (index >= menu->count) return;

    uint8_t previous = menu->focused;
    if (menu->items[previous].button.state == BUTTON_FOCUSED)
        menu->items[previous].button.state = BUTTON_NORMAL;

    menu->focused = index;

    if (menu->items[index].button.state != BUTTON_DISABLED)
        menu->items[index].button.state = BUTTON_FOCUSED;

    render_item(menu, previous);
    render_item(menu, index);
}

MenuResult_t Menu_HandleEvent(Menu_t *menu, NavigationEvent_t event)
{
    switch (event) {

        // -- Keyboard directional navigation --
        case NAVIGATION_LEFT:
        case NAVIGATION_RIGHT:
        case NAVIGATION_UP:
        case NAVIGATION_DOWN: {
            uint8_t next = compute_next_focus(menu, menu->focused, event);
            if (next != menu->focused) {
                Menu_Focus(menu, next);
                return MENU_RESULT_FOCUS;
            }
            return MENU_RESULT_NONE;
        }

        // -- Keyboard confirm (Enter) --
        case NAVIGATION_CONFIRM: {
            MenuItem_t *item = &menu->items[menu->focused];
            if (item->action && item->button.state != BUTTON_DISABLED) {
                item->button.state = BUTTON_PRESSED;
                render_item(menu, menu->focused);
                item->action();
                return MENU_RESULT_ACTION;
            }
            return MENU_RESULT_BACK;
        }

        // -- Touch tap --
        case NAVIGATION_TOUCH: {
            int16_t touch_x, touch_y;
            Navigation_GetTouchPosition(&touch_x, &touch_y);
            for (uint8_t i = 0; i < menu->count; i++) {
                MenuItem_t *item = &menu->items[i];
                if (item->button.state == BUTTON_DISABLED) continue;
                if (!Button_HitTest(&item->button, touch_x, touch_y)) continue;
                Menu_Focus(menu, i);
                if (item->action) {
                    item->button.state = BUTTON_PRESSED;
                    render_item(menu, i);
                    item->action();
                    return MENU_RESULT_ACTION;
                }
                return MENU_RESULT_BACK;
            }
            return MENU_RESULT_NONE;
        }

        // -- Escape --
        case NAVIGATION_BACK:
            return MENU_RESULT_BACK;

        default:
            return MENU_RESULT_NONE;
    }
}
