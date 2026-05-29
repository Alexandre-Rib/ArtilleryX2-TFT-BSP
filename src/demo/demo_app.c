/**
 * @file    demo_app.c
 * @brief   BSP showcase demo — 3x2 navigable menu + scene dispatcher
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Menu layout (320x240, 8px gaps):
 *
 *    [0 Image    ][1 Animation][2 Keyboard ]
 *    [3 ---      ][4 ---      ][5 ---      ]
 *
 *  Navigation:
 *    Keyboard arrows  — move focus (wraps within row / column)
 *    Enter            — enter scene
 *    Escape           — back to menu (from inside a scene)
 *    Touch tap        — move focus + enter on same tap
 */

#include "demo_app.h"
#include "ui_button.h"
#include "ui_nav.h"
#include "scene_image.h"
#include "scene_anim.h"
#include "scene_keyboard.h"
#include "scene_calib.h"
#include "keyboard.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "os_timer.h"
#include <stddef.h>

// ---------------------------------------------------------------------------
// Menu geometry — fits exactly in 320x240 with 8px gaps
// ---------------------------------------------------------------------------
#define MENU_COLS   3
#define MENU_ROWS   2
#define MENU_COUNT  (MENU_COLS * MENU_ROWS)

#define BTN_GAP  8
#define BTN_W    ((LCD_WIDTH  - (MENU_COLS + 1) * BTN_GAP) / MENU_COLS)   // 96
#define BTN_H    ((LCD_HEIGHT - (MENU_ROWS + 1) * BTN_GAP) / MENU_ROWS)   // 108

// top-left corner of button [col][row]
#define BTN_X(col)  (BTN_GAP + (col) * (BTN_W + BTN_GAP))
#define BTN_Y(row)  (BTN_GAP + (row) * (BTN_H + BTN_GAP))

// ---------------------------------------------------------------------------
// Scene descriptor
// ---------------------------------------------------------------------------
typedef struct {
    void (*on_enter)(void);
    void (*on_update)(uint32_t now_ms);
    void (*on_exit)(void);
} Scene_t;

// ---------------------------------------------------------------------------
// Button + scene table (index matches: 0=top-left … 5=bottom-right)
// ---------------------------------------------------------------------------
static Button_t buttons[MENU_COUNT] = {
    { BTN_X(0), BTN_Y(0), BTN_W, BTN_H, "Image\nFlash/SD", BTN_NORMAL   },
    { BTN_X(1), BTN_Y(0), BTN_W, BTN_H, "Animation\n+ Sound",  BTN_NORMAL   },
    { BTN_X(2), BTN_Y(0), BTN_W, BTN_H, "Keyboard\nLive",      BTN_NORMAL   },
    { BTN_X(0), BTN_Y(1), BTN_W, BTN_H, "Touch\nCalib",         BTN_NORMAL   },
    { BTN_X(1), BTN_Y(1), BTN_W, BTN_H, "---",                 BTN_DISABLED },
    { BTN_X(2), BTN_Y(1), BTN_W, BTN_H, "---",                 BTN_DISABLED },
};

static const Scene_t scenes[MENU_COUNT] = {
    { SceneImage_OnEnter,    SceneImage_OnUpdate,    SceneImage_OnExit    },
    { SceneAnim_OnEnter,     SceneAnim_OnUpdate,     SceneAnim_OnExit     },
    { SceneKeyboard_OnEnter, SceneKeyboard_OnUpdate, SceneKeyboard_OnExit },
    { SceneCalib_OnEnter,    SceneCalib_OnUpdate,    SceneCalib_OnExit    },
    { NULL, NULL, NULL },
    { NULL, NULL, NULL },
};

// ---------------------------------------------------------------------------
// Runtime state
// ---------------------------------------------------------------------------
static int8_t          focused_idx    = 0;     // 0-5, currently highlighted
static const Scene_t  *current_scene  = NULL;  // NULL = menu visible

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void menu_draw_all(void)
{
    GUI_Clear(BLACK);
    for (int i = 0; i < MENU_COUNT; i++)
        Button_Draw(&buttons[i]);
}

static void set_focus(int8_t idx)
{
    // unfocus previous
    if (buttons[focused_idx].state == BTN_FOCUSED)
        buttons[focused_idx].state = BTN_NORMAL;

    focused_idx = idx;

    if (buttons[focused_idx].state != BTN_DISABLED)
        buttons[focused_idx].state = BTN_FOCUSED;

    // redraw both affected buttons
    Button_Draw(&buttons[idx]);
    // previous was already set to NORMAL above — redraw it too
    // (we can't know old idx here so we redraw all; cheap on this HW)
    menu_draw_all();
}

static void enter_scene(int8_t idx)
{
    if (scenes[idx].on_enter == NULL) return;   // disabled slot
    current_scene = &scenes[idx];
    current_scene->on_enter();
}

// ---------------------------------------------------------------------------
// Menu navigation helpers
// ---------------------------------------------------------------------------

static int8_t nav_move(int8_t idx, NavEvent_t ev)
{
    int8_t col = idx % MENU_COLS;
    int8_t row = idx / MENU_COLS;

    switch (ev) {
        case NAV_RIGHT: col = (col + 1) % MENU_COLS;            break;
        case NAV_LEFT:  col = (col + MENU_COLS - 1) % MENU_COLS; break;
        case NAV_DOWN:  row = (row + 1) % MENU_ROWS;            break;
        case NAV_UP:    row = (row + MENU_ROWS - 1) % MENU_ROWS; break;
        default: return idx;
    }
    int8_t next = (int8_t)(row * MENU_COLS + col);
    // Skip disabled slots — keep current position
    if (buttons[next].state == BTN_DISABLED) return idx;
    return next;
}

static void handle_menu_event(NavEvent_t ev)
{
    switch (ev) {
        case NAV_LEFT:
        case NAV_RIGHT:
        case NAV_UP:
        case NAV_DOWN: {
            int8_t next = nav_move(focused_idx, ev);
            if (next != focused_idx)
                set_focus(next);
            break;
        }
        case NAV_CONFIRM:
            if (buttons[focused_idx].state != BTN_DISABLED) {
                buttons[focused_idx].state = BTN_PRESSED;
                Button_Draw(&buttons[focused_idx]);
                enter_scene(focused_idx);
            }
            break;

        case NAV_TOUCH: {
            int16_t tx, ty;
            Nav_GetTouchPos(&tx, &ty);
            for (int i = 0; i < MENU_COUNT; i++) {
                if (buttons[i].state != BTN_DISABLED &&
                    Button_HitTest(&buttons[i], tx, ty)) {
                    set_focus((int8_t)i);
                    buttons[i].state = BTN_PRESSED;
                    Button_Draw(&buttons[i]);
                    enter_scene((int8_t)i);
                    break;
                }
            }
            break;
        }
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

void DemoApp_Run(void)
{
    Nav_Init();
    Keyboard_Init();

    // Initial focus on button 0
    buttons[focused_idx].state = BTN_FOCUSED;
    menu_draw_all();

    while (1) {
        Keyboard_Process();
        uint32_t now = OS_GetTimeMs();
        NavEvent_t ev = Nav_Poll();

        if (current_scene != NULL) {
            // Inside a scene
            current_scene->on_update(now);

            if (ev == NAV_BACK) {
                current_scene->on_exit();
                current_scene = NULL;
                // Restore menu: reset pressed button to focused
                buttons[focused_idx].state = BTN_FOCUSED;
                menu_draw_all();
            }
        } else {
            // In menu
            handle_menu_event(ev);
        }
    }
}
