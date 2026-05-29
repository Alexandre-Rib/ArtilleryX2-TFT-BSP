/**
 * @file    settings.c
 * @brief   Persistent settings — stored in last W25Q64 sector (SETTINGS_ADDR)
 * @version 1.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-29
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 */

#include "settings.h"
#include "flash_map.h"
#include <string.h>

// Default calibration constants (safe fallback, wide range)
#define DEFAULT_X_MIN  200u
#define DEFAULT_X_MAX  3900u
#define DEFAULT_Y_MIN  200u
#define DEFAULT_Y_MAX  3900u

// ---------------------------------------------------------------------------
// CRC — simple XOR over all bytes except the crc field itself
// ---------------------------------------------------------------------------
static uint16_t compute_crc(const Settings_t *s)
{
    const uint8_t *p   = (const uint8_t *)s;
    uint16_t       crc = 0;
    // All bytes up to (but not including) the crc field
    for (size_t i = 0; i < offsetof(Settings_t, crc); i++)
        crc ^= (uint16_t)p[i];
    return crc;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void Settings_GetDefault(Settings_t *out)
{
    out->magic       = SETTINGS_MAGIC;
    out->touch_x_min = DEFAULT_X_MIN;
    out->touch_x_max = DEFAULT_X_MAX;
    out->touch_y_min = DEFAULT_Y_MIN;
    out->touch_y_max = DEFAULT_Y_MAX;
    out->crc         = compute_crc(out);
}

bool Settings_Load(Settings_t *out)
{
    FlashMap_Read(SETTINGS_ADDR, (uint8_t *)out, sizeof(Settings_t));

    if (out->magic != SETTINGS_MAGIC)        return false;
    if (compute_crc(out) != out->crc)        return false;
    // Sanity: min must be < max, values in 12-bit ADC range
    if (out->touch_x_min >= out->touch_x_max) return false;
    if (out->touch_y_min >= out->touch_y_max) return false;
    if (out->touch_x_max > 4095u)             return false;
    if (out->touch_y_max > 4095u)             return false;

    return true;
}

void Settings_Save(const Settings_t *s)
{
    Settings_t buf;
    memcpy(&buf, s, sizeof(buf));
    buf.magic = SETTINGS_MAGIC;
    buf.crc   = compute_crc(&buf);

    FlashMap_EraseSector(SETTINGS_ADDR);
    FlashMap_Write(SETTINGS_ADDR, (const uint8_t *)&buf, sizeof(buf));
}
