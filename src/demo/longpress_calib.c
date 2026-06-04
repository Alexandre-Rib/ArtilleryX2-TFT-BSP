/**
 * @file  longpress_calib.c
 * @brief Long-press touch trigger for calibration.
 */

#include "longpress_calib.h"
#include "xpt2046.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "font_embedded.h"
#include "mks_tft28.h"

// ---------------------------------------------------------------------------
// Timings
// ---------------------------------------------------------------------------
#define HOLD_COUNTDOWN_MS  25000u   // delay before countdown overlay appears
#define HOLD_TRIGGER_MS    30000u   // total hold to trigger
#define RELEASE_WAIT_MS     2000u   // hold-off after release

// ---------------------------------------------------------------------------
// Overlay box geometry — centered on 320×240
// ---------------------------------------------------------------------------
#define OVL_X    40
#define OVL_Y    72
#define OVL_W   240
#define OVL_H   100
#define OVL_BG  0x0008u   // very dark blue
#define OVL_BC  0x07FFu   // cyan border (2 px)

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
typedef enum {
    S_IDLE,         // pen up or held < 25 s — no overlay
    S_COUNTDOWN,    // 25–30 s hold — digit overlay
    S_WAIT_RELEASE, // ≥30 s hit — "release" message
    S_WAIT_2S,      // released — 2-second hold-off countdown
} State_t;

static State_t   s_state          = S_IDLE;
static uint32_t  s_press_start_ms = 0;
static uint32_t  s_release_ms     = 0;
static uint8_t   s_last_digit     = 0xFF;  // tracks last rendered digit to avoid flicker

// ---------------------------------------------------------------------------
// Overlay helpers
// ---------------------------------------------------------------------------

static void draw_box(void)
{
    GUI_FillRectColor(OVL_X, OVL_Y, OVL_X + OVL_W, OVL_Y + OVL_H, OVL_BG);
    GUI_FillRectColor(OVL_X,             OVL_Y,             OVL_X + OVL_W,   OVL_Y + 2,           OVL_BC);
    GUI_FillRectColor(OVL_X,             OVL_Y + OVL_H - 2, OVL_X + OVL_W,   OVL_Y + OVL_H,       OVL_BC);
    GUI_FillRectColor(OVL_X,             OVL_Y,             OVL_X + 2,        OVL_Y + OVL_H,       OVL_BC);
    GUI_FillRectColor(OVL_X + OVL_W - 2, OVL_Y,            OVL_X + OVL_W,   OVL_Y + OVL_H,       OVL_BC);
}

/* Countdown overlay: "Calibration dans" label + large digit. */
static void draw_countdown(uint8_t digit)
{
    if (digit == s_last_digit) return;
    s_last_digit = digit;

    draw_box();
    Font_DrawStringCentered(OVL_X + 4, OVL_Y + 6, OVL_X + OVL_W - 4, OVL_Y + 18,
                            "Calibration dans", 1, 0x8410u);
    char buf[2] = { (char)('0' + digit), '\0' };
    Font_DrawStringCentered(OVL_X, OVL_Y + 20, OVL_X + OVL_W, OVL_Y + OVL_H - 4,
                            buf, 4, 0xFFE0u);
}

/* "Release your finger, wait 2 s after release." */
static void draw_release_msg(void)
{
    s_last_digit = 0xFF;
    draw_box();
    Font_DrawStringCentered(OVL_X + 4, OVL_Y + 12, OVL_X + OVL_W - 4, OVL_Y + 28,
                            "Relacher l'ecran !", 1, 0xFFFFu);
    Font_DrawStringCentered(OVL_X + 4, OVL_Y + 34, OVL_X + OVL_W - 4, OVL_Y + 50,
                            "Attente 2 sec apres", 1, 0xFFE0u);
    Font_DrawStringCentered(OVL_X + 4, OVL_Y + 54, OVL_X + OVL_W - 4, OVL_Y + 70,
                            "le relachement", 1, 0xFFE0u);
    Font_DrawStringCentered(OVL_X + 4, OVL_Y + 76, OVL_X + OVL_W - 4, OVL_Y + 92,
                            "avant la calibration", 1, 0x8410u);
}

/* 2-second hold-off countdown — shows when calib will start. */
static void draw_wait_2s(uint8_t secs)
{
    if (secs == s_last_digit) return;
    s_last_digit = secs;

    draw_box();
    Font_DrawStringCentered(OVL_X + 4, OVL_Y + 6, OVL_X + OVL_W - 4, OVL_Y + 18,
                            "Calibration dans", 1, 0x8410u);
    char buf[3] = { (char)('0' + secs), 's', '\0' };
    Font_DrawStringCentered(OVL_X, OVL_Y + 20, OVL_X + OVL_W, OVL_Y + OVL_H - 4,
                            buf, 4, 0x07E0u);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void LongpressCalib_Init(void)
{
    s_state          = S_IDLE;
    s_press_start_ms = 0;
    s_release_ms     = 0;
    s_last_digit     = 0xFF;
}

void LongpressCalib_Reset(void)
{
    s_state      = S_IDLE;
    s_last_digit = 0xFF;
    s_press_start_ms = 0;
}

bool LongpressCalib_IsBlocking(void)
{
    return s_state != S_IDLE;
}

LpcStatus_t LongpressCalib_Update(uint32_t now_ms)
{
    bool pen_down = (XPT2046_Read_Pen() == 0);

    switch (s_state) {

    case S_IDLE:
        if (pen_down) {
            if (s_press_start_ms == 0)
                s_press_start_ms = now_ms;
            if ((now_ms - s_press_start_ms) >= HOLD_COUNTDOWN_MS) {
                s_state      = S_COUNTDOWN;
                s_last_digit = 0xFF;
            }
        } else {
            s_press_start_ms = 0;
        }
        break;

    case S_COUNTDOWN:
        if (!pen_down) {
            /* Released before trigger — abort, signal redraw needed. */
            s_state          = S_IDLE;
            s_press_start_ms = 0;
            s_last_digit     = 0xFF;
            return LPC_CANCELLED;
        }
        {
            uint32_t held    = now_ms - s_press_start_ms;
            uint32_t remain  = (held < HOLD_TRIGGER_MS) ? (HOLD_TRIGGER_MS - held) : 0u;
            if (remain == 0u) {
                /* 30 s reached. */
                s_state = S_WAIT_RELEASE;
                draw_release_msg();
            } else {
                /* digit = ceiling(remain / 1000), clamped to [0,5]. */
                uint8_t digit = (uint8_t)((remain + 999u) / 1000u);
                if (digit > 5u) digit = 5u;
                draw_countdown(digit);
            }
        }
        break;

    case S_WAIT_RELEASE:
        if (!pen_down) {
            s_release_ms = now_ms;
            s_state      = S_WAIT_2S;
            s_last_digit = 0xFF;
            draw_wait_2s(2u);
        }
        break;

    case S_WAIT_2S:
        {
            uint32_t elapsed = now_ms - s_release_ms;
            if (elapsed >= RELEASE_WAIT_MS) {
                s_state          = S_IDLE;
                s_press_start_ms = 0;
                return LPC_LAUNCH;
            }
            uint32_t remain = RELEASE_WAIT_MS - elapsed;
            uint8_t  secs   = (uint8_t)((remain + 999u) / 1000u);
            if (secs > 2u) secs = 2u;
            draw_wait_2s(secs);
        }
        break;
    }

    return LPC_IDLE;
}
