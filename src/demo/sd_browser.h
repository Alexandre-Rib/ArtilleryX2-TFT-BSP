/**
 * @file    sd_browser.h
 * @brief   Generic SD card directory browser, extension-filtered
 * @version 1.0
 * @date    Created: 2026-05-31
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  Stateful singleton: one active browser at a time.
 *  Manages path and item list; the caller owns selection, scroll and drawing.
 *
 *  Usage:
 *    static const char *const exts[] = {"snd"};
 *    SdBrowser_Init(exts, 1);
 *    SdBrowser_Mount();
 *    // draw SdBrowser_GetItemCount() rows via SdBrowser_GetItem(i)
 *    SdBrowser_EnterDir(idx);   // navigate into a directory
 *    SdBrowser_GoUp();          // navigate to parent
 *    SdBrowser_Unmount();
 */

#ifndef _SD_BROWSER_H_
#define _SD_BROWSER_H_

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define SDBROW_NAME_LEN   24u
#define SDBROW_PATH_LEN   64u
#define SDBROW_MAX_ITEMS  24u

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
typedef enum {
    SDBROW_ITEM_PARENT,   ///< ".." — go-up entry, shown only when not at root
    SDBROW_ITEM_DIR,      ///< Sub-directory
    SDBROW_ITEM_FILE,     ///< File matching at least one accepted extension
} SdBrowItemType_t;

typedef struct {
    SdBrowItemType_t type;
    char             name[SDBROW_NAME_LEN];
} SdBrowItem_t;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

/**
 * @brief  Set the accepted file extensions (case-insensitive, without dot).
 *
 * Must be called before SdBrowser_Mount().
 * The caller owns the @p exts array; it must remain valid for the browser's
 * lifetime (static arrays work best).
 *
 * Examples:
 *   static const char *const snd_exts[] = {"snd"};
 *   SdBrowser_Init(snd_exts, 1);
 *
 *   static const char *const img_exts[] = {"bmp", "jpg"};
 *   SdBrowser_Init(img_exts, 2);
 */
void SdBrowser_Init(const char * const *exts, uint8_t ext_count);

/**
 * @brief  Mount the SD card and scan the root directory.
 * @return true if the card is accessible.
 */
bool SdBrowser_Mount(void);

/** Unmount the SD card. No-op if not mounted. */
void SdBrowser_Unmount(void);

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------
bool        SdBrowser_IsMounted(void);
bool        SdBrowser_IsAtRoot(void);
const char *SdBrowser_GetPath(void);           ///< e.g. "0:/res/sound/"
uint8_t     SdBrowser_GetItemCount(void);
const SdBrowItem_t *SdBrowser_GetItem(uint8_t idx);  ///< NULL if out of range

/** Last f_opendir FRESULT value, cast to int.  0 = FR_OK. */
int         SdBrowser_GetLastError(void);

// ---------------------------------------------------------------------------
// Navigation  (automatically rescans after each move)
// ---------------------------------------------------------------------------

/**
 * @brief  Navigate into the directory at list index @p idx.
 * @note   @p idx must point to a SDBROW_ITEM_DIR entry.
 *         SDBROW_ITEM_PARENT entries are handled by SdBrowser_GoUp().
 */
void SdBrowser_EnterDir(uint8_t idx);

/** Navigate up one level.  No-op at root. */
void SdBrowser_GoUp(void);

// ---------------------------------------------------------------------------
// File path helper
// ---------------------------------------------------------------------------

/**
 * @brief  Build the full path for a FILE item: current_path + name.
 * @param[in]  idx       List index of a SDBROW_ITEM_FILE entry.
 * @param[out] out       Destination buffer.
 * @param[in]  out_size  Size of @p out in bytes.
 * @return true on success; false if idx is out of range, not a file,
 *         or the resulting path would overflow @p out.
 */
bool SdBrowser_GetFilePath(uint8_t idx, char *out, uint16_t out_size);

#endif /* _SD_BROWSER_H_ */
