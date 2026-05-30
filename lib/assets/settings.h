/**
 * @file    settings.h
 * @brief   Persistent settings — stored in the last W25Q64 sector
 * @version 2.0
 * @date    Created:       2026-05-29
 *          Last modified: 2026-05-30
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Flash layout (SETTINGS_ADDR, one sector):
 *    [0]  uint32_t magic       — SETTINGS_MAGIC when the sector contains valid data
 *    [4]  uint16_t touch_x_min — raw ADC value at the left edge
 *    [6]  uint16_t touch_x_max — raw ADC value at the right edge
 *    [8]  uint16_t touch_y_min — raw ADC value at the top edge
 *    [10] uint16_t touch_y_max — raw ADC value at the bottom edge
 *    [12] uint16_t crc         — XOR checksum of all preceding bytes
 *
 *  Typical usage:
 *    Settings_t settings;
 *    if (!Settings_Load(&settings)) Settings_GetDefault(&settings);
 *    Navigation_SetTouchCalibration(settings.touch_x_min, settings.touch_x_max,
 *                                   settings.touch_y_min, settings.touch_y_max);
 */

#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdint.h>
#include <stdbool.h>

#define SETTINGS_MAGIC  0xBEEFCAFEu   ///< Sentinel value written at the start of every valid sector.

/**
 * @brief  All persistent settings stored in external flash.
 */
typedef struct {
    uint32_t magic;         ///< Must equal SETTINGS_MAGIC for the struct to be considered valid.
    uint16_t touch_x_min;  ///< Smallest raw ADC X value seen at the left screen edge.
    uint16_t touch_x_max;  ///< Largest raw ADC X value seen at the right screen edge.
    uint16_t touch_y_min;  ///< Smallest raw ADC Y value seen at the top screen edge.
    uint16_t touch_y_max;  ///< Largest raw ADC Y value seen at the bottom screen edge.
    uint16_t crc;           ///< XOR checksum of all bytes that precede this field.
} Settings_t;

/**
 * @brief  Load settings from external flash.
 *
 * Reads the settings sector, validates the magic number and XOR checksum,
 * then checks that calibration ranges are sane (min < max, values in 12-bit ADC range).
 *
 * @param[out] out  Populated with the stored values when the function returns true.
 * @return true   Flash contains valid settings; @p out is ready to use.
 * @return false  No valid settings found; call Settings_GetDefault() to get safe values.
 */
bool Settings_Load(Settings_t *out);

/**
 * @brief  Save settings to external flash.
 *
 * Erases the settings sector, then writes the struct with an updated magic
 * field and a freshly computed CRC.  Any previously stored settings are lost.
 *
 * @param[in] settings  Values to persist.
 */
void Settings_Save(const Settings_t *settings);

/**
 * @brief  Fill a Settings_t with safe compile-time defaults.
 *
 * Use this when Settings_Load() returns false (no valid calibration in flash).
 * The default values cover the full 12-bit ADC range to ensure the touch screen
 * is usable, though accuracy will be poor until calibrated.
 *
 * @param[out] out  Filled with default calibration values.
 */
void Settings_GetDefault(Settings_t *out);

#endif
