/**
 * @file    demo_app.c
 * @brief   BSP showcase demo — main menu + scene dispatcher
 * @version 3.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Navigation pattern (see ui_menu.h for the full specification):
 *    - Main menu is a Menu_t with 6 items in a 3x2 grid.
 *    - Keyboard arrows move focus (cyan aura); Enter activates.
 *    - Touch tap focuses + activates in one gesture.
 *    - NAVIGATION_BACK on the main menu has no effect (root, no parent).
 *    - Each scene can call DemoApp_RequestExit() to return to the main menu.
 *
 *  Scene lifecycle:  on_enter() -> on_update(now_ms, event) -> on_exit()
 *    on_update returns true  -> scene consumed NAVIGATION_BACK internally
 *    on_update returns false -> demo_app handles NAVIGATION_BACK (exits scene)
 *
 *  Resource installation (one-time at first boot):
 *    ResInstaller_Run() copies BMP files from SD /res/pic/ to W25Q64.
 *    If resources are already installed the call returns immediately.
 *    If the SD card is absent, icon buttons display a fallback cross.
 */

#include "demo_app.h"
#include "ui_menu.h"
#include "ui_button.h"
#include "ui_nav.h"
#include "img_draw.h"
#include "res_map.h"
#include "res_installer.h"
#include "font_embedded.h"
#include "settings.h"
#include "scene_image.h"
#include "scene_anim.h"
#include "scene_keyboard.h"
#include "scene_calib.h"
#include "keyboard.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "os_timer.h"
#include <stddef.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Scene descriptor
// ---------------------------------------------------------------------------
typedef struct {
    void (*on_enter)(void);
    bool (*on_update)(uint32_t now_ms, NavigationEvent_t event);
    void (*on_exit)(void);
} Scene_t;

// ---------------------------------------------------------------------------
// Main menu geometry — 3x2 grid filling 320x240 with 8-pixel gaps
// ---------------------------------------------------------------------------
#define MENU_COLS  3
#define MENU_ROWS  2
#define BTN_GAP    8
#define BTN_W      ((LCD_WIDTH  - (MENU_COLS + 1) * BTN_GAP) / MENU_COLS)   // 96
#define BTN_H      ((LCD_HEIGHT - (MENU_ROWS + 1) * BTN_GAP) / MENU_ROWS)   // 108

#define BTN_X(col) (BTN_GAP + (col) * (BTN_W + BTN_GAP))
#define BTN_Y(row) (BTN_GAP + (row) * (BTN_H + BTN_GAP))

// Image area inside each button:
//   icon  80x80 centered horizontally, 4px from top
//   label remaining space at the bottom
#define ICON_X_OFF  ((BTN_W - (int16_t)RES_IMG_W) / 2)   // 8 px
#define ICON_Y_OFF  4
#define LABEL_Y_OFF (ICON_Y_OFF + (int16_t)RES_IMG_H + 2) // just below icon

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void action_open_image(void);
static void action_open_sound(void);
static void action_open_anim(void);
static void action_open_calib(void);
static void action_open_keyboard(void);

static void draw_btn_0(bool focused);
static void draw_btn_1(bool focused);
static void draw_btn_2(bool focused);
static void draw_btn_3(bool focused);
static void draw_btn_4(bool focused);
static void draw_btn_5(bool focused);

// ---------------------------------------------------------------------------
// Main menu — 6 items, 3 columns
// ---------------------------------------------------------------------------
// Grid layout (3×2):
//   [0 IMAGE ][1 SOUND][2 ANIM ]
//   [3 CALIB ][4 KEYS ][5 ---  ]
static MenuItem_t main_items[6] = {
    { { BTN_X(0), BTN_Y(0), BTN_W, BTN_H, NULL, BUTTON_NORMAL   }, action_open_image,    draw_btn_0 },
    { { BTN_X(1), BTN_Y(0), BTN_W, BTN_H, NULL, BUTTON_NORMAL   }, action_open_sound,    draw_btn_1 },
    { { BTN_X(2), BTN_Y(0), BTN_W, BTN_H, NULL, BUTTON_NORMAL   }, action_open_anim,     draw_btn_2 },
    { { BTN_X(0), BTN_Y(1), BTN_W, BTN_H, NULL, BUTTON_NORMAL   }, action_open_calib,    draw_btn_3 },
    { { BTN_X(1), BTN_Y(1), BTN_W, BTN_H, NULL, BUTTON_NORMAL   }, action_open_keyboard, draw_btn_4 },
    { { BTN_X(2), BTN_Y(1), BTN_W, BTN_H, NULL, BUTTON_DISABLED }, NULL,                 draw_btn_5 },
};

static Menu_t main_menu = {
    .items   = main_items,
    .count   = 6,
    .cols    = MENU_COLS,
    .focused = 0,
    .parent  = NULL,
};

// ---------------------------------------------------------------------------
// Scene table — indexed by main_items slot
// ---------------------------------------------------------------------------
static const Scene_t scenes[6] = {
    { SceneImage_OnEnter,    SceneImage_OnUpdate,    SceneImage_OnExit    }, // 0 IMAGE
    { SceneAnim_OnEnter,     SceneAnim_OnUpdate,     SceneAnim_OnExit     }, // 1 SOUND  (uses anim scene — stub)
    { SceneAnim_OnEnter,     SceneAnim_OnUpdate,     SceneAnim_OnExit     }, // 2 ANIM
    { SceneCalib_OnEnter,    SceneCalib_OnUpdate,    SceneCalib_OnExit    }, // 3 CALIB
    { SceneKeyboard_OnEnter, SceneKeyboard_OnUpdate, SceneKeyboard_OnExit }, // 4 KEYS
    { NULL, NULL, NULL },                                                     // 5 ---
};

// ---------------------------------------------------------------------------
// Runtime state
// ---------------------------------------------------------------------------
static int8_t active_scene_index = -1;
static bool   exit_requested     = false;

// ---------------------------------------------------------------------------
// Icon button renderer — shared by all 6 draw functions
// ---------------------------------------------------------------------------
static void menu_draw_btn(uint8_t idx, uint8_t img_slot,
                           bool enabled, const char *label, bool focused)
{
    int16_t bx = main_items[idx].button.x;
    int16_t by = main_items[idx].button.y;

    // Background
    uint16_t bg = enabled ? 0x2104u : 0x18C3u;
    GUI_FillRectColor((uint16_t)bx,        (uint16_t)by,
                      (uint16_t)(bx+BTN_W), (uint16_t)(by+BTN_H), bg);

    // Border / focus ring
    if (focused) {
        uint16_t c = 0x07FFu;   // cyan — 2px ring
        GUI_FillRectColor((uint16_t)bx,         (uint16_t)by,          (uint16_t)(bx+BTN_W),   (uint16_t)(by+2),      c);
        GUI_FillRectColor((uint16_t)bx,         (uint16_t)(by+BTN_H-2),(uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)bx,         (uint16_t)by,          (uint16_t)(bx+2),        (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)(bx+BTN_W-2),(uint16_t)by,         (uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
    } else {
        uint16_t c = 0x4228u;   // subtle — 1px border
        GUI_FillRectColor((uint16_t)bx,         (uint16_t)by,          (uint16_t)(bx+BTN_W),   (uint16_t)(by+1),      c);
        GUI_FillRectColor((uint16_t)bx,         (uint16_t)(by+BTN_H-1),(uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)bx,         (uint16_t)by,          (uint16_t)(bx+1),        (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)(bx+BTN_W-1),(uint16_t)by,         (uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
    }

    // Icon
    int16_t img_x = bx + ICON_X_OFF;
    int16_t img_y = by + ICON_Y_OFF;

    if (enabled)
        ImgDraw_FromFlash(img_slot, img_x, img_y);
    else
        ImgDraw_Cross(img_x, img_y, 0x18C3u);

    // Label — embedded font scale=1 (8px), centered in bottom area
    int16_t lbl_y0 = by + LABEL_Y_OFF;
    int16_t lbl_y1 = by + BTN_H;
    uint16_t lbl_c = enabled ? WHITE : 0x528Au;
    Font_DrawStringCentered(bx, lbl_y0, bx + BTN_W, lbl_y1, label, 1, lbl_c);
}

static void draw_btn_0(bool f) { menu_draw_btn(0, RES_IMG_PICTURE,     true,  "IMAGE", f); }
static void draw_btn_1(bool f) { menu_draw_btn(1, RES_IMG_SOUND,       true,  "SOUND", f); }
static void draw_btn_2(bool f) { menu_draw_btn(2, RES_IMG_ANIMATION,   true,  "ANIM",  f); }
static void draw_btn_3(bool f) { menu_draw_btn(3, RES_IMG_CALIBRATION, true,  "CALIB", f); }
static void draw_btn_4(bool f) { menu_draw_btn(4, RES_IMG_KEYBOARD,    true,  "KEYS",  f); }
static void draw_btn_5(bool f) { menu_draw_btn(5, RES_IMG_UNDEF,       false, "---",   f); }

// ---------------------------------------------------------------------------
// Scene transitions
// ---------------------------------------------------------------------------
static void enter_scene(int8_t index)
{
    if (index < 0 || index >= 6) return;
    if (!scenes[index].on_enter) return;

    active_scene_index = index;
    exit_requested     = false;
    scenes[index].on_enter();
}

static void exit_scene(void)
{
    if (active_scene_index >= 0 && scenes[active_scene_index].on_exit)
        scenes[active_scene_index].on_exit();

    active_scene_index = -1;
    exit_requested     = false;

    main_items[main_menu.focused].button.state = BUTTON_FOCUSED;
    GUI_Clear(BLACK);
    Menu_Draw(&main_menu);
}

static void action_open_image(void)    { enter_scene(0); }
static void action_open_sound(void)    { enter_scene(1); }
static void action_open_anim(void)     { enter_scene(2); }
static void action_open_calib(void)    { enter_scene(3); }
static void action_open_keyboard(void) { enter_scene(4); }

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void DemoApp_RequestExit(void)
{
    exit_requested = true;
}

// Run the calibration scene in a blocking loop until the user saves or skips.
static void run_calib_blocking(void)
{
    SceneCalib_OnEnter();
    for (;;) {
        Keyboard_Process();
        NavigationEvent_t event  = Navigation_Poll();
        bool              consumed = SceneCalib_OnUpdate(OS_GetTimeMs(), event);

        Settings_t test;
        if (Settings_Load(&test)) break;                     // saved — done

        if (!consumed && event == NAVIGATION_BACK) break;    // ESC in LIVE = skip
    }
    SceneCalib_OnExit();
}

void DemoApp_Run(void)
{
    Navigation_Init();
    Keyboard_Init();

    // Install resources from SD if a fresh "res/" directory is present.
    bool just_installed = ResInstaller_Run();

    // Load or acquire touch calibration.
    Settings_t cfg;
    if (Settings_Load(&cfg)) {
        Navigation_SetTouchCalibration(cfg.touch_x_min, cfg.touch_x_max,
                                       cfg.touch_y_min, cfg.touch_y_max);
    } else if (just_installed) {
        // First install ever — no calibration in flash yet.
        // Run calibration now so the result screen touch is accurate.
        run_calib_blocking();
        if (Settings_Load(&cfg))
            Navigation_SetTouchCalibration(cfg.touch_x_min, cfg.touch_x_max,
                                           cfg.touch_y_min, cfg.touch_y_max);
    }

    // Show install result (blocks until screen press; skipped if all OK).
    if (just_installed)
        ResInstaller_ShowResult();

    main_items[0].button.state = BUTTON_FOCUSED;
    GUI_Clear(BLACK);
    Menu_Draw(&main_menu);

    while (1) {
        Keyboard_Process();
        uint32_t          now_ms = OS_GetTimeMs();
        NavigationEvent_t event  = Navigation_Poll();

        if (active_scene_index >= 0) {
            bool consumed = scenes[active_scene_index].on_update(now_ms, event);

            if (exit_requested || (event == NAVIGATION_BACK && !consumed))
                exit_scene();
        } else {
            MenuResult_t result = Menu_HandleEvent(&main_menu, event);
            (void)result;
        }
    }
}
