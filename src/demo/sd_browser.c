/**
 * @file    sd_browser.c
 * @brief   Generic SD card directory browser, extension-filtered
 * @version 1.0
 * @date    Created: 2026-05-31
 * @note    Developed with Claude Sonnet 4.6 (Anthropic)
 *
 *  FatFS R0.14 note: trailing slash on non-root paths causes FR_INVALID_NAME
 *  in create_name() when it encounters an empty component after the slash.
 *  "0:/" is valid (root).  "0:/res/" is not — must use "0:/res".
 *  This module always strips the trailing slash before calling f_opendir.
 */

#include "sd_browser.h"
#include "ff.h"
#include "sd.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
static const char * const *s_exts;
static uint8_t              s_ext_count;

static FATFS         s_fatfs;
static bool          s_mounted;
static int           s_last_error;

static bool          s_cd_prev;        // card state at last poll
static uint32_t      s_cd_next_ms;     // next time to check card detect

static char          s_path[SDBROW_PATH_LEN];
static SdBrowItem_t  s_items[SDBROW_MAX_ITEMS];
static uint8_t       s_item_count;

// ---------------------------------------------------------------------------
// Extension matching — case-insensitive, without dot
// ---------------------------------------------------------------------------
static bool ext_matches(const char *fname)
{
    uint16_t len = (uint16_t)strlen(fname);
    if (len < 2u) return false;

    int16_t dot = -1;
    for (int16_t i = (int16_t)(len - 1u); i >= 0; i--) {
        if (fname[i] == '.') { dot = i; break; }
    }
    if (dot < 0) return false;
    const char *ext = fname + dot + 1;

    for (uint8_t i = 0; i < s_ext_count; i++) {
        const char *ref = s_exts[i];
        uint8_t j = 0;
        while (ref[j] && ext[j]) {
            char a = ext[j], b = ref[j];
            if (a >= 'A' && a <= 'Z') a += 0x20;
            if (b >= 'A' && b <= 'Z') b += 0x20;
            if (a != b) goto next_ext;
            j++;
        }
        if (ref[j] == '\0' && ext[j] == '\0') return true;
    next_ext:;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Directory scan into s_items[]
// ---------------------------------------------------------------------------
static void scan_dir(void)
{
    s_item_count = 0;

    if (strlen(s_path) > 3u) {
        s_items[0].type = SDBROW_ITEM_PARENT;
        memcpy(s_items[0].name, "..", 3u);
        s_item_count = 1;
    }

    if (!s_mounted) return;

    // Strip trailing slash on non-root paths (FatFS R0.14 limitation).
    char open_path[SDBROW_PATH_LEN];
    uint16_t plen = (uint16_t)strlen(s_path);
    if (plen > 3u && s_path[plen - 1u] == '/') {
        memcpy(open_path, s_path, plen - 1u);
        open_path[plen - 1u] = '\0';
    } else {
        memcpy(open_path, s_path, plen + 1u);
    }

    DIR     dir;
    FILINFO fno;
    s_last_error = (int)f_opendir(&dir, open_path);
    if (s_last_error != 0) return;

    while (s_item_count < SDBROW_MAX_ITEMS) {
        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == '\0') break;
        if (fno.fattrib & AM_DIR) {
            s_items[s_item_count].type = SDBROW_ITEM_DIR;
        } else {
            if (!ext_matches(fno.fname)) continue;
            s_items[s_item_count].type = SDBROW_ITEM_FILE;
        }
        strncpy(s_items[s_item_count].name, fno.fname, SDBROW_NAME_LEN - 1u);
        s_items[s_item_count].name[SDBROW_NAME_LEN - 1u] = '\0';
        s_item_count++;
    }
    f_closedir(&dir);

    // Sort: dirs before files.  PARENT at [0] is untouched.
    // Insertion sort that bubbles each DIR to the front of the unsorted region.
    uint8_t sorted = (s_item_count > 0u && s_items[0].type == SDBROW_ITEM_PARENT) ? 1u : 0u;
    for (uint8_t i = sorted; i < s_item_count; i++) {
        if (s_items[i].type == SDBROW_ITEM_DIR) {
            SdBrowItem_t tmp = s_items[i];
            for (uint8_t j = i; j > sorted; j--) s_items[j] = s_items[j - 1u];
            s_items[sorted] = tmp;
            sorted++;
        }
    }
}

// ---------------------------------------------------------------------------
// Public API — lifecycle
// ---------------------------------------------------------------------------
void SdBrowser_Init(const char * const *exts, uint8_t ext_count)
{
    s_exts        = exts;
    s_ext_count   = ext_count;
    s_mounted     = false;
    s_last_error  = 0;
    s_item_count  = 0;
    s_cd_prev     = false;
    s_cd_next_ms  = 0u;
    memcpy(s_path, "0:/", 4u);
}

bool SdBrowser_Mount(void)
{
    s_mounted = false;
    memcpy(s_path, "0:/", 4u);
    if (f_mount(&s_fatfs, "0:/", 1) == FR_OK) {
        s_mounted = true;
        scan_dir();
    }
    // f_mount → disk_initialize → SD_Init → SD_SPI_Init: CD pin is now valid
    s_cd_prev = (bool)SD_CD_Inserted();
    return s_mounted;
}

void SdBrowser_Unmount(void)
{
    if (s_mounted) {
        f_mount(NULL, "0:/", 0);
        s_mounted = false;
    }
}

// ---------------------------------------------------------------------------
// Public API — state queries
// ---------------------------------------------------------------------------
bool        SdBrowser_IsMounted(void)  { return s_mounted; }
bool        SdBrowser_IsAtRoot(void)   { return strlen(s_path) <= 3u; }
const char *SdBrowser_GetPath(void)    { return s_path; }
uint8_t     SdBrowser_GetItemCount(void) { return s_item_count; }
int         SdBrowser_GetLastError(void) { return s_last_error; }

const SdBrowItem_t *SdBrowser_GetItem(uint8_t idx)
{
    if (idx >= s_item_count) return (const SdBrowItem_t *)0;
    return &s_items[idx];
}

// ---------------------------------------------------------------------------
// Public API — navigation
// ---------------------------------------------------------------------------
void SdBrowser_EnterDir(uint8_t idx)
{
    if (idx >= s_item_count) return;
    if (s_items[idx].type != SDBROW_ITEM_DIR) return;

    uint16_t plen = (uint16_t)strlen(s_path);
    uint16_t nlen = (uint16_t)strlen(s_items[idx].name);
    if (plen + nlen + 2u >= SDBROW_PATH_LEN) return;
    memcpy(s_path + plen, s_items[idx].name, nlen);
    s_path[plen + nlen]     = '/';
    s_path[plen + nlen + 1] = '\0';
    scan_dir();
}

void SdBrowser_GoUp(void)
{
    uint16_t len = (uint16_t)strlen(s_path);
    if (len <= 3u) return;
    len--;                                          // skip trailing '/'
    len--;                                          // last char of dir name
    while (len > 2u && s_path[len] != '/') len--;  // find previous '/'
    s_path[len + 1u] = '\0';
    scan_dir();
}

// ---------------------------------------------------------------------------
// Public API — file path helper
// ---------------------------------------------------------------------------
bool SdBrowser_GetFilePath(uint8_t idx, char *out, uint16_t out_size)
{
    if (idx >= s_item_count) return false;
    if (s_items[idx].type != SDBROW_ITEM_FILE) return false;

    uint16_t plen = (uint16_t)strlen(s_path);
    uint16_t nlen = (uint16_t)strlen(s_items[idx].name);
    if (plen + nlen + 1u > out_size) return false;
    memcpy(out, s_path, plen);
    memcpy(out + plen, s_items[idx].name, nlen + 1u);
    return true;
}

// ---------------------------------------------------------------------------
// Hot-plug detection
// ---------------------------------------------------------------------------
#define CD_POLL_MS  500u

bool SdBrowser_Poll(uint32_t now_ms)
{
    if (now_ms < s_cd_next_ms) return false;
    s_cd_next_ms = now_ms + CD_POLL_MS;

    bool inserted = (bool)SD_CD_Inserted();
    if (inserted == s_cd_prev) return false;
    s_cd_prev = inserted;

    if (inserted) {
        memcpy(s_path, "0:/", 4u);
        s_mounted = false;
        if (f_mount(&s_fatfs, "0:/", 1) == FR_OK) {
            s_mounted = true;
            scan_dir();
        }
    } else {
        f_mount(NULL, "0:/", 0);
        s_mounted    = false;
        s_item_count = 0;
        memcpy(s_path, "0:/", 4u);
    }
    return true;
}
