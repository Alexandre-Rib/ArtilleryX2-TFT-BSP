/**
 * @file    scene_mouse.c
 * @brief   Scene: souris USB HID — placeholder en attente d'implémentation
 */

#include "scene_mouse.h"
#include "demo_app.h"
#include "ui_nav.h"
#include "font_embedded.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "mks_tft28.h"
#include <stdbool.h>

#define FOOTER_H   26
#define FOOTER_Y0  (LCD_HEIGHT - FOOTER_H)
#define C_BACK_BG  0x8800u
#define C_BACK_FG  0xFFFFu

void SceneMouse_OnEnter(void)
{
    GUI_Clear(BLACK);

    GUI_FillRectColor(0, 0, LCD_WIDTH, 20, 0x000Fu);
    Font_DrawStringCentered(0, 0, LCD_WIDTH, 20, "MOUSE", 1, WHITE);

    Font_DrawStringCentered(0, LCD_HEIGHT / 2 - 16,
                             LCD_WIDTH, LCD_HEIGHT / 2,
                             "COMING SOON", 2, 0x4228u);

    GUI_FillRectColor(0, FOOTER_Y0, LCD_WIDTH, LCD_HEIGHT, C_BACK_BG);
    Font_DrawStringCentered(0, FOOTER_Y0, LCD_WIDTH, LCD_HEIGHT, "BACK", 1, C_BACK_FG);
}

bool SceneMouse_OnUpdate(uint32_t now_ms, NavigationEvent_t event)
{
    (void)now_ms;

    if (event == NAVIGATION_TOUCH) {
        int16_t tx, ty;
        Navigation_GetTouchPosition(&tx, &ty);
        if (ty >= FOOTER_Y0) {
            DemoApp_RequestExit();
            return false;
        }
    }

    return false;   // ESC propagé vers SceneControllers
}

void SceneMouse_OnExit(void) {}
