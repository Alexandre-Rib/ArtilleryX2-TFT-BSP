/**
 * @file    scene_joystick.c
 * @brief   Scene: affichage état manette Mega Drive en temps réel via MEGA9
 *
 *  Layout (320×240):
 *    y=  0.. 19 : titre "JOYSTICK"
 *    y= 20.. 37 : barre de statut (CONNECTED / NO CONTROLLER / LINK DEAD)
 *    y= 38.. 39 : séparateur
 *    y= 40..151 : grille 8 boutons en 2 colonnes × 4 lignes (28 px/ligne)
 *    y=152..213 : espace
 *    y=214..239 : pied-de-page BACK
 *
 *  Colonne gauche (D-pad) : UP / DOWN / LEFT / RIGHT
 *  Colonne droite (boutons): A  / B    / C    / START
 *
 *  Chaque case change de couleur quand le bouton est maintenu.
 */

#include "scene_joystick.h"
#include "demo_app.h"
#include "ui_nav.h"
#include "settings.h"
#include "font_embedded.h"
#include "mega9.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "mks_tft28.h"
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
#define TITLE_H      20
#define STATUS_H     18
#define SEP_H         2
#define GRID_ROW_H   28      // 4 lignes × 28 px = 112 px
#define GRID_ROWS     4
#define FOOTER_H     26
#define FOOTER_Y0    (LCD_HEIGHT - FOOTER_H)

#define TITLE_Y1     TITLE_H
#define STATUS_Y0    TITLE_Y1
#define STATUS_Y1    (STATUS_Y0 + STATUS_H)
#define SEP_Y0       STATUS_Y1
#define GRID_Y0      (SEP_Y0 + SEP_H)

// Deux colonnes séparées par 4 px au centre
#define COL_GAP       4
#define COL_W         ((LCD_WIDTH - COL_GAP) / 2)   // 158
#define COL_L_X0      0
#define COL_L_X1      COL_W
#define COL_R_X0      (COL_W + COL_GAP)
#define COL_R_X1      LCD_WIDTH

// ---------------------------------------------------------------------------
// Couleurs
// ---------------------------------------------------------------------------
#define C_TITLE_BG    0x000Fu
#define C_TITLE_FG    0xFFFFu
#define C_SEP         0x4228u
#define C_CONN_BG     0x0400u   // vert foncé — manette connectée
#define C_DISC_BG     0x2104u   // gris — pas de manette
#define C_DEAD_BG     0x8000u   // rouge foncé — lien mort
#define C_STATUS_FG   0xFFFFu
#define C_BTN_OFF     0x0821u   // fond bouton relâché
#define C_BTN_ON      0x07E0u   // fond bouton appuyé (vert vif)
#define C_BTN_FG_OFF  0x8410u   // texte relâché (gris)
#define C_BTN_FG_ON   0x0000u   // texte appuyé (noir sur vert)
#define C_BACK_BG     0x8800u
#define C_BACK_FG     0xFFFFu

// ---------------------------------------------------------------------------
// Description d'une touche dans la grille
// ---------------------------------------------------------------------------
typedef struct {
    uint8_t     mask;   // MEGA9_BTN_*
    const char *name;
    uint8_t     col;    // 0 = gauche, 1 = droite
    uint8_t     row;    // 0..3
} BtnDesc_t;

static const BtnDesc_t k_btns[8] = {
    { MEGA9_BTN_UP,    "UP",    0, 0 },
    { MEGA9_BTN_DOWN,  "DOWN",  0, 1 },
    { MEGA9_BTN_LEFT,  "LEFT",  0, 2 },
    { MEGA9_BTN_RIGHT, "RIGHT", 0, 3 },
    { MEGA9_BTN_A,     "A",     1, 0 },
    { MEGA9_BTN_B,     "B",     1, 1 },
    { MEGA9_BTN_C,     "C",     1, 2 },
    { MEGA9_BTN_START, "START", 1, 3 },
};

static uint8_t s_prev_buttons;
static bool    s_prev_connected;
static bool    s_prev_link;

// ---------------------------------------------------------------------------
// Dessin
// ---------------------------------------------------------------------------
static void draw_btn(uint8_t i, bool pressed)
{
    const BtnDesc_t *b  = &k_btns[i];
    int16_t x0 = (b->col == 0) ? COL_L_X0 : COL_R_X0;
    int16_t x1 = (b->col == 0) ? COL_L_X1 : COL_R_X1;
    int16_t y0 = (int16_t)(GRID_Y0 + b->row * GRID_ROW_H);
    int16_t y1 = y0 + GRID_ROW_H;

    uint16_t bg = pressed ? C_BTN_ON  : C_BTN_OFF;
    uint16_t fg = pressed ? C_BTN_FG_ON : C_BTN_FG_OFF;

    GUI_FillRectColor((uint16_t)x0, (uint16_t)y0, (uint16_t)x1, (uint16_t)(y1 - 1), bg);
    // Séparateur horizontal bas de cellule (1 px noir)
    GUI_FillRectColor((uint16_t)x0, (uint16_t)(y1 - 1), (uint16_t)x1, (uint16_t)y1, BLACK);

    Font_DrawStringCentered(x0, y0, x1, y1 - 1, b->name, 1, fg);
}

static void draw_status(void)
{
    uint16_t bg;
    const char *msg;

    if (!Mega9_IsLinkAlive()) {
        bg  = C_DEAD_BG;
        msg = "LINK DEAD";
    } else if (!Mega9_IsConnected()) {
        bg  = C_DISC_BG;
        msg = "NO CONTROLLER";
    } else {
        bg  = C_CONN_BG;
        msg = "CONNECTED";
    }

    GUI_FillRectColor(0, STATUS_Y0, LCD_WIDTH, STATUS_Y1, bg);
    Font_DrawStringCentered(0, STATUS_Y0, LCD_WIDTH, STATUS_Y1, msg, 1, C_STATUS_FG);
}

static void draw_static_layout(void)
{
    GUI_Clear(BLACK);

    GUI_FillRectColor(0, 0, LCD_WIDTH, TITLE_Y1, C_TITLE_BG);
    Font_DrawStringCentered(0, 0, LCD_WIDTH, TITLE_Y1, "JOYSTICK", 1, C_TITLE_FG);

    draw_status();

    GUI_FillRectColor(0, SEP_Y0, LCD_WIDTH, SEP_Y0 + SEP_H, C_SEP);

    // Séparateur vertical entre les deux colonnes
    GUI_FillRectColor(COL_L_X1, GRID_Y0, COL_R_X0, FOOTER_Y0, BLACK);

    uint8_t btns = Mega9_GetButtons();
    for (uint8_t i = 0; i < 8u; i++)
        draw_btn(i, (btns & k_btns[i].mask) != 0u);

    GUI_FillRectColor(0, FOOTER_Y0, LCD_WIDTH, LCD_HEIGHT, C_BACK_BG);
    Font_DrawStringCentered(0, FOOTER_Y0, LCD_WIDTH, LCD_HEIGHT, "BACK", 1, C_BACK_FG);
}

// ---------------------------------------------------------------------------
// API publique
// ---------------------------------------------------------------------------

void SceneJoystick_OnEnter(void)
{
    s_prev_buttons   = 0xFFu;
    s_prev_connected = Mega9_IsConnected();
    s_prev_link      = Mega9_IsLinkAlive();
    draw_static_layout();
}

bool SceneJoystick_OnUpdate(uint32_t now_ms, NavigationEvent_t event)
{
    (void)now_ms;

    // Block gamepad A from exiting the diagnostic when calibration data is in flash.
    // If no calibration exists, A is the fallback exit (touch footer may be unreliable).
#ifndef DIAG_BACK_UNLOCKED
    if (event == NAVIGATION_BACK) {
        Settings_t _cfg;
        if (Settings_Load(&_cfg))
            return true;
    }
#endif

    // Touch dans le pied-de-page → retour
    if (event == NAVIGATION_TOUCH) {
        int16_t tx, ty;
        Navigation_GetTouchPosition(&tx, &ty);
        if (ty >= FOOTER_Y0) {
            DemoApp_RequestExit();
            return false;
        }
    }

    // Mise à jour statut — les 3 états (CONNECTED / NO CONTROLLER / LINK DEAD)
    // nécessitent de tracker IsConnected ET IsLinkAlive séparément :
    // les deux derniers valent tous les deux IsConnected()==false.
    bool connected = Mega9_IsConnected();
    bool link      = Mega9_IsLinkAlive();
    if (connected != s_prev_connected || link != s_prev_link) {
        s_prev_connected = connected;
        s_prev_link      = link;
        draw_status();
        s_prev_buttons = 0xFFu;   // redraw boutons aussi (état peut avoir changé)
    }

    // Mise à jour de l'état des boutons (rafraîchissement partiel)
    uint8_t btns = Mega9_GetButtons();
    if (btns != s_prev_buttons) {
        uint8_t changed = btns ^ s_prev_buttons;
        for (uint8_t i = 0; i < 8u; i++) {
            if (changed & k_btns[i].mask)
                draw_btn(i, (btns & k_btns[i].mask) != 0u);
        }
        s_prev_buttons = btns;
    }

    return false;   // ESC propagé vers SceneControllers
}

void SceneJoystick_OnExit(void) {}
