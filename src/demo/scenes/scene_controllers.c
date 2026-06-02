/**
 * @file    scene_controllers.c
 * @brief   Scene: sous-menu Controllers (3 boutons — Clavier / Souris / Joystick)
 *
 *  Layout (320×240):
 *    y=  0..13 : bandeau titre "CONTROLLERS"
 *    y= 14..65 : espace noir supérieur
 *    y= 66..173: rangée de 3 boutons 96×108 (icône 80×80 + label)
 *    y=174..239: espace noir inférieur
 *
 *  Connectivité dynamique (détection plug/unplug à la volée) :
 *    - KEYBOARD : Keyboard_IsConnected()  — USB HID
 *    - MOUSE    : toujours non-dispo (placeholder)
 *    - JOYSTICK : Mega9_IsConnected()     — UART Mega Drive
 *
 *  Comportement lorsqu'un périphérique est débranché :
 *    - Bouton passe BUTTON_DISABLED, affiche icône RES_IMG_UNPLUGGED
 *    - Le focus se déplace automatiquement vers un bouton disponible
 *    - Si tous sont indisponibles, focus reste sur slot 0 (BUTTON_DISABLED)
 *
 *  Icône manquante → chaîne générique via ImgDraw_FromFlash :
 *    slot → undef_menu.bmp → croix noire
 */

#include "scene_controllers.h"
#include "scene_keyboard.h"
#include "scene_mouse.h"
#include "scene_joystick.h"
#include "demo_app.h"
#include "ui_menu.h"
#include "ui_button.h"
#include "ui_nav.h"
#include "img_draw.h"
#include "res_map.h"
#include "font_embedded.h"
#include "keyboard.h"
#include "mega9.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "mks_tft28.h"
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Géométrie — identique au menu principal (BTN_W=96, BTN_H=108, GAP=8)
// ---------------------------------------------------------------------------
#define BTN_W      96
#define BTN_H     108
#define BTN_GAP     8
#define BTN_Y0     66
#define BTN_X(c)  (BTN_GAP + (c) * (BTN_W + BTN_GAP))

#define TITLE_H   14
#define C_TITLE_BG  0x000Fu
#define C_TITLE_FG  0xFFFFu

// ---------------------------------------------------------------------------
// État interne
// ---------------------------------------------------------------------------
typedef enum { STATE_MENU, STATE_KBD, STATE_MOUSE, STATE_JOY } CtrlState_t;

static CtrlState_t s_state;
static bool        s_prev_kbd;
static bool        s_prev_mse;
static bool        s_prev_joy;

// ---------------------------------------------------------------------------
// Dessin d'un bouton du sous-menu
//   img_name  → nom de l'image (ex. "keyboard", "unplugged_device")
//   available → false = style grisé, focus ring masqué
// ---------------------------------------------------------------------------
static void ctrl_draw_btn(uint8_t idx, const char *img_name,
                           bool available, bool focused)
{
    int16_t bx = (int16_t)BTN_X(idx);
    int16_t by = BTN_Y0;

    // Fond : différent selon disponibilité
    uint16_t bg = available ? 0x2104u : 0x18C3u;
    GUI_FillRectColor((uint16_t)bx, (uint16_t)by,
                      (uint16_t)(bx + BTN_W), (uint16_t)(by + BTN_H), bg);

    // Bordure : focus (cyan) / non-disponible (absent) / normal (gris)
    if (focused && available) {
        uint16_t c = 0x07FFu;
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)by,           (uint16_t)(bx+BTN_W),   (uint16_t)(by+2),      c);
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)(by+BTN_H-2), (uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)by,           (uint16_t)(bx+2),        (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)(bx+BTN_W-2), (uint16_t)by,           (uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
    } else if (!available) {
        // Bouton grisé : bordure encore plus sombre (quasiment invisible)
        uint16_t c = 0x2104u;
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)by,           (uint16_t)(bx+BTN_W),   (uint16_t)(by+1),      c);
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)(by+BTN_H-1), (uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)by,           (uint16_t)(bx+1),        (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)(bx+BTN_W-1), (uint16_t)by,           (uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
    } else {
        // Normal non-focalisé
        uint16_t c = 0x4228u;
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)by,           (uint16_t)(bx+BTN_W),   (uint16_t)(by+1),      c);
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)(by+BTN_H-1), (uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)bx,           (uint16_t)by,           (uint16_t)(bx+1),        (uint16_t)(by+BTN_H),  c);
        GUI_FillRectColor((uint16_t)(bx+BTN_W-1), (uint16_t)by,           (uint16_t)(bx+BTN_W),   (uint16_t)(by+BTN_H),  c);
    }

    // Icône — chaîne complète slot → undef_menu → croix noire
    int16_t icon_x = bx + (BTN_W - (int16_t)RES_IMG_W) / 2;
    int16_t icon_y = by + 4;
    ImgDraw_ByName(img_name, icon_x, icon_y);

    // Label
    int16_t lbl_y0 = icon_y + (int16_t)RES_IMG_H + 2;
    uint16_t fg = available ? WHITE : 0x528Au;
    Font_DrawStringCentered(bx, lbl_y0, bx + BTN_W, by + BTN_H,
                             (idx == 0) ? "KEYBOARD" :
                             (idx == 1) ? "MOUSE"    : "JOYSTICK", 1, fg);
}

// ---------------------------------------------------------------------------
// Wrappers draw : choisissent l'icône selon la disponibilité courante
// ---------------------------------------------------------------------------
static void draw_kbd(bool f)
{
    bool conn = Keyboard_IsConnected();
    ctrl_draw_btn(0, conn ? "keyboard" : "unplugged_device", conn, f);
}
static void draw_mse(bool f)
{
    bool conn = Mouse_IsConnected();
    ctrl_draw_btn(1, conn ? "mouse" : "unplugged_device", conn, f);
}
static void draw_joy(bool f)
{
    bool conn = Mega9_IsConnected();
    ctrl_draw_btn(2, conn ? "joystick" : "unplugged_device", conn, f);
}

// ---------------------------------------------------------------------------
// Actions (appelées par Menu_HandleEvent à l'activation)
// ---------------------------------------------------------------------------
static void open_keyboard(void)
{
    if (!Keyboard_IsConnected()) return;   // bloqué si débranché
    SceneKeyboard_OnEnter();
    s_state = STATE_KBD;
}
static void open_mouse(void)
{
    if (!Mouse_IsConnected()) return;
    SceneMouse_OnEnter();
    s_state = STATE_MOUSE;
}
static void open_joystick(void)
{
    if (!Mega9_IsConnected()) return;      // bloqué si débranché
    SceneJoystick_OnEnter();
    s_state = STATE_JOY;
}

// ---------------------------------------------------------------------------
// Menu_t
// ---------------------------------------------------------------------------
static MenuItem_t s_items[3] = {
    { { BTN_X(0), BTN_Y0, BTN_W, BTN_H, NULL, BUTTON_NORMAL }, open_keyboard, draw_kbd },
    { { BTN_X(1), BTN_Y0, BTN_W, BTN_H, NULL, BUTTON_NORMAL }, open_mouse,    draw_mse },
    { { BTN_X(2), BTN_Y0, BTN_W, BTN_H, NULL, BUTTON_NORMAL }, open_joystick, draw_joy },
};

static Menu_t s_menu = {
    .items   = s_items,
    .count   = 3,
    .cols    = 3,
    .focused = 0,
    .parent  = NULL,
};

// ---------------------------------------------------------------------------
// Mise à jour des états boutons selon la connectivité courante.
// Déplace le focus si le bouton actuellement focalisé devient indisponible.
// ---------------------------------------------------------------------------
static void sync_states(bool kbd, bool mse, bool joy)
{
    bool avail[3] = { kbd, mse, joy };

    // Mise à jour des états (sans écraser le focus)
    for (uint8_t i = 0; i < 3u; i++) {
        if (s_items[i].button.state == BUTTON_FOCUSED) continue;  // préserver focus
        s_items[i].button.state = avail[i] ? BUTTON_NORMAL : BUTTON_DISABLED;
    }

    // Si le bouton focalisé vient de devenir indisponible → déplacer le focus
    uint8_t f = s_menu.focused;
    if (!avail[f]) {
        // Recherche du premier bouton disponible
        uint8_t new_f = 0xFFu;
        for (uint8_t i = 0; i < 3u; i++) {
            if (avail[i]) { new_f = i; break; }
        }

        if (new_f != 0xFFu) {
            // Déplacer le focus
            s_items[f].button.state   = BUTTON_DISABLED;
            s_items[new_f].button.state = BUTTON_FOCUSED;
            s_menu.focused            = new_f;
        } else {
            // Aucun bouton disponible : laisser le focus sur slot 0 mais désactivé
            s_items[f].button.state = BUTTON_DISABLED;
        }
    }
}

// ---------------------------------------------------------------------------
// Dessin complet du sous-menu
// ---------------------------------------------------------------------------
static void draw_menu(void)
{
    GUI_Clear(BLACK);
    GUI_FillRectColor(0, 0, LCD_WIDTH, TITLE_H, C_TITLE_BG);
    Font_DrawStringCentered(0, 0, LCD_WIDTH, TITLE_H, "CONTROLLERS", 1, C_TITLE_FG);
    Menu_Draw(&s_menu);

    // USB diagnostic — zone noire sous les boutons (y=174..200)
    GUI_FillRectColor(0, 174, LCD_WIDTH, 200, BLACK);
    Font_DrawStringCentered(0, 178, LCD_WIDTH, 196, USB_GetDiagStr(), 1, 0xFFFFu);
}

// ---------------------------------------------------------------------------
// API publique
// ---------------------------------------------------------------------------

void SceneControllers_OnEnter(void)
{
    s_state    = STATE_MENU;
    s_prev_kbd = Keyboard_IsConnected();
    s_prev_mse = Mouse_IsConnected();
    s_prev_joy = Mega9_IsConnected();

    s_items[0].button.state = s_prev_kbd ? BUTTON_NORMAL : BUTTON_DISABLED;
    s_items[1].button.state = s_prev_mse ? BUTTON_NORMAL : BUTTON_DISABLED;
    s_items[2].button.state = s_prev_joy ? BUTTON_NORMAL : BUTTON_DISABLED;

    // Focus : premier bouton disponible
    s_menu.focused = 0;
    for (uint8_t i = 0; i < 3u; i++) {
        if (s_items[i].button.state == BUTTON_NORMAL) {
            s_items[i].button.state = BUTTON_FOCUSED;
            s_menu.focused          = i;
            break;
        }
    }

    draw_menu();
}

bool SceneControllers_OnUpdate(uint32_t now_ms, NavigationEvent_t event)
{
    // ------------------------------------------------------------------
    // Mode MENU
    // ------------------------------------------------------------------
    if (s_state == STATE_MENU) {

        // Détection plug/unplug à la volée
        bool kbd = Keyboard_IsConnected();
        bool mse = Mouse_IsConnected();
        bool joy = Mega9_IsConnected();

        bool state_changed = (kbd != s_prev_kbd || mse != s_prev_mse || joy != s_prev_joy);
        if (state_changed) {
            s_prev_kbd = kbd;
            s_prev_mse = mse;
            s_prev_joy = joy;
            sync_states(kbd, mse, joy);
            draw_menu();
        } else {
            // Rafraîchit le diagnostic USB toutes les ~500 ms sans tout redessiner
            static uint32_t s_diag_ms = 0;
            if (now_ms - s_diag_ms >= 500u) {
                s_diag_ms = now_ms;
                GUI_FillRectColor(0, 174, LCD_WIDTH, 200, BLACK);
                Font_DrawStringCentered(0, 178, LCD_WIDTH, 196,
                                        USB_GetDiagStr(), 1, 0xFFFFu);
            }
        }

        Menu_HandleEvent(&s_menu, event);
        // MENU_RESULT_BACK → return false (demo_app sort vers le menu principal)
        return false;
    }

    // ------------------------------------------------------------------
    // Sous-scène active : déléguer puis intercepter le retour
    // ------------------------------------------------------------------
    bool consumed = false;
    switch (s_state) {
    case STATE_KBD:   consumed = SceneKeyboard_OnUpdate (now_ms, event); break;
    case STATE_MOUSE: consumed = SceneMouse_OnUpdate    (now_ms, event); break;
    case STATE_JOY:   consumed = SceneJoystick_OnUpdate (now_ms, event); break;
    default: break;
    }

    bool sub_wants_exit = DemoApp_IsExitRequested()
                       || (!consumed && event == NAVIGATION_BACK);

    if (sub_wants_exit) {
        DemoApp_CancelExit();

        switch (s_state) {
        case STATE_KBD:   SceneKeyboard_OnExit();  break;
        case STATE_MOUSE: SceneMouse_OnExit();     break;
        case STATE_JOY:   SceneJoystick_OnExit();  break;
        default: break;
        }

        s_prev_kbd = Keyboard_IsConnected();
        s_prev_mse = Mouse_IsConnected();
        s_prev_joy = Mega9_IsConnected();
        sync_states(s_prev_kbd, s_prev_mse, s_prev_joy);

        s_state = STATE_MENU;
        draw_menu();
        return true;    // on a consommé le back
    }

    return true;    // sous-scène active : consommer tous les événements
}

void SceneControllers_OnExit(void)
{
    switch (s_state) {
    case STATE_KBD:   SceneKeyboard_OnExit();  break;
    case STATE_MOUSE: SceneMouse_OnExit();     break;
    case STATE_JOY:   SceneJoystick_OnExit();  break;
    default: break;
    }
    s_state = STATE_MENU;
}
