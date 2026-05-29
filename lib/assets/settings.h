/**
 * @file    settings.h
 * @brief   Persistent settings — stored in last W25Q64 sector (SETTINGS_ADDR)
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Layout in flash:
 *    [0]  uint32_t magic       — SETTINGS_MAGIC when valid
 *    [4]  uint16_t touch_x_min
 *    [6]  uint16_t touch_x_max
 *    [8]  uint16_t touch_y_min
 *    [10] uint16_t touch_y_max
 *    [12] uint16_t crc         — XOR of all preceding bytes
 *
 *  Usage:
 *    Settings_t s;
 *    if (!Settings_Load(&s)) Settings_GetDefault(&s);
 *    Nav_SetCalibration(s.touch_x_min, s.touch_x_max,
 *                       s.touch_y_min, s.touch_y_max);
 */

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdint.h>
#include <stdbool.h>

#define SETTINGS_MAGIC  0xBEEFCAFEu

typedef struct {
    uint32_t magic;
    uint16_t touch_x_min;
    uint16_t touch_x_max;
    uint16_t touch_y_min;
    uint16_t touch_y_max;
    uint16_t crc;
} Settings_t;

// Load settings from flash. Returns false if no valid data (use defaults).
bool Settings_Load(Settings_t *out);

// Erase the settings sector and write new values.
void Settings_Save(const Settings_t *s);

// Populate s with safe compile-time defaults.
void Settings_GetDefault(Settings_t *out);

#endif
