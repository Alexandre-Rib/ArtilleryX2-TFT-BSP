/**
 * @file    res_installer.c
 * @brief   Resource installer — scan dynamique /res/pic/.bmp → W25Q64
 * @version 3.0  (BSP3: header nom 16 o par slot, plus de liste fixe)
 *
 *  Déclencheur : présence du répertoire "res/" à la racine de la SD.
 *  Après installation réussie, renommé en "res_cur/" → pas de ré-install au boot.
 *
 *  Format slot image (BSP3) :
 *    [0..15]  char name[16]  — nom de fichier sans extension, majuscules, null-padded
 *    [16..]   RGB565 pixels  — RES_IMG_W × RES_IMG_H × 2 (12 800 octets)
 *
 *  Toute image ≤ 80×80 en 24-bit BGR non compressé est acceptée (centrée + padding noir).
 */

#include "res_installer.h"
#include "res_map.h"
#include "flash_map.h"
#include "font_embedded.h"
#include "GUI.h"
#include "LCD_Colors.h"
#include "mks_tft28.h"
#include "keyboard.h"
#include "xpt2046.h"
#include "ff.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Types et helpers locaux
// ---------------------------------------------------------------------------
typedef enum {
    SLOT_OK        = 0u,
    SLOT_WAR_DIM   = 1u,
    SLOT_MISS      = 2u,
    SLOT_ERR_FMT   = 3u,
    SLOT_ERR_DIM   = 4u,
    SLOT_ERR_IO    = 5u,
    SLOT_ERR_FLASH = 6u,
} SlotStatus_t;

#define LE16(p) ((uint16_t)((p)[0] | ((uint16_t)(p)[1] << 8)))
#define LE32(p) ((uint32_t)((p)[0] | ((uint32_t)(p)[1]<<8) | \
                             ((uint32_t)(p)[2]<<16) | ((uint32_t)(p)[3]<<24)))

// Résultats par fichier (remplis pendant Run, lus pendant ShowResult)
#define FNAME_DISP_LEN  20u
static struct {
    char         fname[FNAME_DISP_LEN];   // nom de fichier (sans extension)
    SlotStatus_t status;
} s_results[RES_IMG_MAX_SLOTS];
static uint8_t s_result_count;

// Liste des .bmp trouvés sur la SD
#define BMP_FNAME_MAX  33u   // longueur max du nom de fichier + null
static char    s_bmp_files[RES_IMG_MAX_SLOTS][BMP_FNAME_MAX];
static uint8_t s_bmp_count;

// ---------------------------------------------------------------------------
// Accumulateur page W25Q64
// ---------------------------------------------------------------------------
static uint8_t s_row_buf[RES_IMG_W * 3u];   // une ligne BGR24
static uint8_t s_page_buf[FLASH_PAGE_SIZE];

static uint32_t s_base_addr;
static uint32_t s_byte_offset;
static uint16_t s_page_pos;

static FATFS s_fs;

static void page_push(uint8_t b)
{
    s_page_buf[s_page_pos++] = b;
    if (s_page_pos >= FLASH_PAGE_SIZE) {
        FlashMap_Write(s_base_addr + s_byte_offset, s_page_buf, FLASH_PAGE_SIZE);
        s_byte_offset += FLASH_PAGE_SIZE;
        s_page_pos     = 0;
    }
}

static void page_flush(void)
{
    if (s_page_pos > 0) {
        FlashMap_Write(s_base_addr + s_byte_offset, s_page_buf, s_page_pos);
        s_byte_offset += s_page_pos;
        s_page_pos     = 0;
    }
}

// ---------------------------------------------------------------------------
// Utilitaire : nom de fichier → header 16 octets (majuscules, null-padded)
// ---------------------------------------------------------------------------
static void extract_img_name(const char *fname, char out[RES_IMG_NAME_LEN])
{
    memset(out, 0, RES_IMG_NAME_LEN);
    for (uint8_t i = 0; i < (uint8_t)RES_IMG_NAME_LEN; i++) {
        char c = fname[i];
        if (c == '.' || c == '\0') break;
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        out[i] = c;
    }
}

// ---------------------------------------------------------------------------
// Détection fichier .bmp
// ---------------------------------------------------------------------------
static bool is_bmp_file(const FILINFO *fno)
{
    if (fno->fattrib & AM_DIR) return false;
    size_t len = strlen(fno->fname);
    if (len < 5u) return false;
    const char *ext = fno->fname + len - 4u;
    return ext[0] == '.' &&
           (ext[1] == 'b' || ext[1] == 'B') &&
           (ext[2] == 'm' || ext[2] == 'M') &&
           (ext[3] == 'p' || ext[3] == 'P');
}

// ---------------------------------------------------------------------------
// Scan des .bmp dans /res/pic/ → s_bmp_files[], s_bmp_count
// ---------------------------------------------------------------------------
static void collect_bmp_files(void)
{
    s_bmp_count = 0;
    DIR     dir;
    FILINFO fno;
    if (f_opendir(&dir, "0:/res/pic") != FR_OK) return;
    for (;;) {
        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == '\0') break;
        if (!is_bmp_file(&fno)) continue;
        if (s_bmp_count >= RES_IMG_MAX_SLOTS) break;
        strncpy(s_bmp_files[s_bmp_count], fno.fname, BMP_FNAME_MAX - 1u);
        s_bmp_files[s_bmp_count][BMP_FNAME_MAX - 1u] = '\0';
        s_bmp_count++;
    }
    f_closedir(&dir);
}

// ---------------------------------------------------------------------------
// Décodage BMP → pixels RGB565 dans le slot (écrit à partir de s_base_addr)
// ---------------------------------------------------------------------------
static SlotStatus_t install_bmp_pixels(FIL *fp)
{
    static uint8_t hdr[54];
    UINT br;
    if (f_read(fp, hdr, 54u, &br) != FR_OK || br < 54u) return SLOT_ERR_IO;
    if (hdr[0] != 'B' || hdr[1] != 'M')                 return SLOT_ERR_FMT;

    uint32_t pixel_offset = LE32(hdr + 10);
    int32_t  raw_w        = (int32_t)LE32(hdr + 18);
    int32_t  raw_h        = (int32_t)LE32(hdr + 22);
    uint16_t bpp          = LE16(hdr + 28);
    uint32_t compression  = LE32(hdr + 30);

    if (bpp != 24u || compression != 0u) return SLOT_ERR_FMT;

    int32_t img_w  = raw_w;
    int32_t img_h  = (raw_h < 0) ? -raw_h : raw_h;
    bool    bot_up = (raw_h > 0);

    if (img_w > (int32_t)RES_IMG_W || img_h > (int32_t)RES_IMG_H) return SLOT_ERR_DIM;

    bool needs_pad = (img_w < (int32_t)RES_IMG_W || img_h < (int32_t)RES_IMG_H);

    uint32_t row_bytes  = (uint32_t)img_w * 3u;
    uint32_t row_stride = (row_bytes + 3u) & ~3u;

    int32_t pad_left  = ((int32_t)RES_IMG_W - img_w) / 2;
    int32_t pad_right = (int32_t)RES_IMG_W - img_w - pad_left;
    int32_t pad_top   = ((int32_t)RES_IMG_H - img_h) / 2;

    s_byte_offset = 0;
    s_page_pos    = 0;

    for (int32_t vis_row = 0; vis_row < (int32_t)RES_IMG_H; vis_row++) {
        int32_t img_row = vis_row - pad_top;

        if (img_row < 0 || img_row >= img_h) {
            for (int32_t x = 0; x < (int32_t)RES_IMG_W; x++) { page_push(0); page_push(0); }
        } else {
            uint32_t file_row = bot_up ? (uint32_t)(img_h - 1 - img_row) : (uint32_t)img_row;
            uint32_t seek_pos = pixel_offset + file_row * row_stride;

            if (f_lseek(fp, (FSIZE_t)seek_pos) != FR_OK)                          return SLOT_ERR_IO;
            if (f_read(fp, s_row_buf, row_bytes, &br) != FR_OK || br != row_bytes) return SLOT_ERR_IO;

            for (int32_t x = 0; x < pad_left;  x++) { page_push(0); page_push(0); }
            for (int32_t x = 0; x < img_w; x++) {
                uint8_t  b  = s_row_buf[(uint32_t)x * 3u + 0u];
                uint8_t  g  = s_row_buf[(uint32_t)x * 3u + 1u];
                uint8_t  r  = s_row_buf[(uint32_t)x * 3u + 2u];
                uint16_t px = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
                page_push((uint8_t)(px >> 8));
                page_push((uint8_t)(px & 0xFFu));
            }
            for (int32_t x = 0; x < pad_right; x++) { page_push(0); page_push(0); }
        }
    }

    page_flush();
    if (s_byte_offset < RES_IMG_BYTES) return SLOT_ERR_IO;
    return needs_pad ? SLOT_WAR_DIM : SLOT_OK;
}

// ---------------------------------------------------------------------------
// Effacement d'un slot image
// ---------------------------------------------------------------------------
static void erase_slot(uint8_t slot)
{
    uint32_t base = RES_IMG_ADDR(slot);
    for (uint32_t off = 0; off < RES_IMG_SLOT_SIZE; off += W25QXX_SECTOR_SIZE)
        FlashMap_EraseSector(base + off);
}

// ---------------------------------------------------------------------------
// Installation d'un fichier BMP dans un slot (header + pixels)
// ---------------------------------------------------------------------------
static SlotStatus_t install_slot(uint8_t slot, const char *fname)
{
    // Vérification bornes flash
    if (RES_IMG_ADDR(slot) + RES_IMG_SLOT_SIZE > SETTINGS_ADDR) return SLOT_ERR_FLASH;

    // Écriture du header nom (16 octets)
    char name_hdr[RES_IMG_NAME_LEN];
    extract_img_name(fname, name_hdr);
    FlashMap_Write(RES_IMG_ADDR(slot), (uint8_t *)name_hdr, RES_IMG_NAME_LEN);

    // Chemin complet "0:/res/pic/<fname>"
    char path[64];
    const char *prefix = "0:/res/pic/";
    uint8_t     plen   = 11u;
    memcpy(path, prefix, plen);
    strncpy(path + plen, fname, sizeof(path) - plen - 1u);
    path[sizeof(path) - 1u] = '\0';

    FIL fp;
    if (f_open(&fp, path, FA_READ) != FR_OK) return SLOT_MISS;

    // Les pixels commencent après le header
    s_base_addr = RES_IMG_PIX_ADDR(slot);
    SlotStatus_t st = install_bmp_pixels(&fp);
    f_close(&fp);
    return st;
}

// ---------------------------------------------------------------------------
// Affichage progression
// ---------------------------------------------------------------------------
static void show_progress(uint8_t current, uint8_t total, const char *label)
{
    if (total == 0u) return;
    int16_t bar_w = (int16_t)(((uint32_t)(current + 1u) * (uint32_t)LCD_WIDTH)
                               / (uint32_t)total);
    GUI_FillRectColor(0,            (uint16_t)(LCD_HEIGHT/2 - 4),
                      (uint16_t)bar_w, (uint16_t)(LCD_HEIGHT/2 + 4), 0x07E0u);
    GUI_FillRectColor((uint16_t)bar_w, (uint16_t)(LCD_HEIGHT/2 - 4),
                      LCD_WIDTH,       (uint16_t)(LCD_HEIGHT/2 + 4), 0x2104u);
    GUI_FillRectColor(0, (uint16_t)(LCD_HEIGHT/2 + 10),
                      LCD_WIDTH, (uint16_t)(LCD_HEIGHT/2 + 26), BLACK);
    Font_DrawStringCentered(0, LCD_HEIGHT/2 + 10, LCD_WIDTH, LCD_HEIGHT/2 + 26,
                             label, 1, WHITE);
}

// ---------------------------------------------------------------------------
// Affichage résultat (slots en erreur seulement)
// ---------------------------------------------------------------------------
static void show_result(void)
{
    static const struct { const char *label; uint16_t color; } STATUS_INFO[] = {
        { "OK",        0x07E0u },
        { "WAR_DIM",   0xFD20u },
        { "MISS",      0x8410u },
        { "ERR_FMT",   0xF800u },
        { "ERR_DIM",   0xF800u },
        { "ERR_IO",    0xF800u },
        { "ERR_FLASH", 0xF800u },
    };

    uint8_t items[RES_IMG_MAX_SLOTS];
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_result_count; i++) {
        if (s_results[i].status != SLOT_OK) items[n++] = i;
    }
    if (n == 0u) return;

    int16_t area_y0 = 30;
    int16_t area_y1 = 195;
    int16_t item_h  = 20;
    uint8_t visible = (uint8_t)((area_y1 - area_y0) / item_h);
    int16_t hint_y0 = 205;
    int16_t hint_y1 = 236;

    uint8_t scroll = 0;
    bool    redraw = true;

    while (!XPT2046_Read_Pen()) {}

    for (;;) {
        if (redraw) {
            redraw = false;
            GUI_Clear(BLACK);
            Font_DrawStringCentered(0, 4, LCD_WIDTH, 22, "INSTALL RESULT", 2, 0x07FFu);
            GUI_FillRectColor(8, 26, LCD_WIDTH - 8, 27, 0x4228u);

            uint8_t show = (uint8_t)(n - scroll);
            if (show > visible) show = visible;

            for (uint8_t r = 0; r < show; r++) {
                uint8_t i  = items[scroll + r];
                uint8_t st = (uint8_t)s_results[i].status;
                if (st >= 7u) st = 5u;
                int16_t y = area_y0 + (int16_t)(r * item_h);
                Font_DrawString(8,   y + 4, s_results[i].fname,     1, WHITE);
                Font_DrawString(180, y,     STATUS_INFO[st].label, 2, STATUS_INFO[st].color);
            }

            if (scroll > 0)
                Font_DrawStringCentered(LCD_WIDTH - 20, area_y0,
                                         LCD_WIDTH, area_y0 + 16, "^", 2, 0x07FFu);
            if ((uint8_t)(scroll + visible) < n)
                Font_DrawStringCentered(LCD_WIDTH - 20, (int16_t)(area_y1 - 16),
                                         LCD_WIDTH, area_y1, "v", 2, 0x07FFu);

            GUI_FillRectColor(8, (uint16_t)(hint_y0 - 4),
                              LCD_WIDTH - 8, (uint16_t)(hint_y0 - 3), 0x4228u);
            Font_DrawStringCentered(0, hint_y0, LCD_WIDTH, hint_y1,
                                     "Press screen to continue", 1, 0x8410u);
        }

        Keyboard_Process();
        if (Keyboard_HasNewKey()) {
            uint8_t key = Keyboard_GetKeycode();
            if      (key == KB_KEY_UP   && scroll > 0)                        { scroll--; redraw = true; }
            else if (key == KB_KEY_DOWN && (uint8_t)(scroll + visible) < n)   { scroll++; redraw = true; }
            else    { return; }
        }
        if (!XPT2046_Read_Pen()) { return; }
    }
}

// ---------------------------------------------------------------------------
// Helpers son (inchangés par rapport à BSP2)
// ---------------------------------------------------------------------------
static void erase_snd_slot(uint8_t slot)
{
    FlashMap_EraseSector(RES_SND_ADDR(slot));
}

static void extract_base_name(const char *fname, char out[8])
{
    memset(out, 0, 8u);
    for (uint8_t i = 0; i < 8u && fname[i] != '.' && fname[i] != '\0'; i++) {
        char c = fname[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        out[i] = c;
    }
}

static bool slot_name_matches(uint8_t slot, const char base[8])
{
    char hdr[8];
    FlashMap_Read(RES_SND_ADDR(slot), (uint8_t *)hdr, 8u);
    if ((uint8_t)hdr[0] == 0xFFu) return false;
    return memcmp(hdr, base, 8u) == 0;
}

static bool slot_is_empty(uint8_t slot)
{
    uint8_t first = 0;
    FlashMap_Read(RES_SND_ADDR(slot), &first, 1u);
    return first == 0xFFu;
}

static bool is_snd_file(const FILINFO *fno)
{
    if (fno->fattrib & AM_DIR) return false;
    uint16_t len = (uint16_t)strlen(fno->fname);
    if (len < 4u) return false;
    const char *ext = fno->fname + len - 4u;
    return ext[0] == '.' &&
           (ext[1] == 's' || ext[1] == 'S') &&
           (ext[2] == 'n' || ext[2] == 'N') &&
           (ext[3] == 'd' || ext[3] == 'D');
}

static SlotStatus_t install_snd_slot(uint8_t slot, const char *fname)
{
    char path[40];
    memcpy(path, "0:/res/sound/", 13u);
    strncpy(path + 13, fname, sizeof(path) - 14u);
    path[sizeof(path) - 1u] = '\0';

    FIL  fp;
    UINT br;
    if (f_open(&fp, path, FA_READ) != FR_OK) return SLOT_MISS;

    s_base_addr   = RES_SND_ADDR(slot);
    s_byte_offset = 0;
    s_page_pos    = 0;

    char name_hdr[8] = {0};
    uint8_t nlen = 0;
    for (uint8_t j = 0; j < 8u && fname[j] != '.' && fname[j] != '\0'; j++) {
        char c = fname[j];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        name_hdr[nlen++] = c;
    }
    for (uint8_t j = 0; j < 8u; j++) page_push((uint8_t)name_hdr[j]);

    bool has_data = false;
    for (;;) {
        uint8_t tmp[4];
        if (f_read(&fp, tmp, 4u, &br) != FR_OK || br < 4u) break;
        uint16_t freq = (uint16_t)(tmp[0] | ((uint16_t)tmp[1] << 8u));
        if (freq == 0xFFFFu) break;
        if (s_byte_offset + s_page_pos + 8u > RES_SND_SLOT_SIZE) break;
        for (uint8_t j = 0; j < 4u; j++) page_push(tmp[j]);
        has_data = true;
    }
    f_close(&fp);
    page_push(0xFFu); page_push(0xFFu); page_push(0xFFu); page_push(0xFFu);
    page_flush();
    return has_data ? SLOT_OK : SLOT_MISS;
}

// ---------------------------------------------------------------------------
// Suppression récursive de répertoire
// ---------------------------------------------------------------------------
static void rmdir_recursive(const char *path)
{
    char path_buf[128];
    size_t plen = strlen(path);
    if (plen >= sizeof(path_buf)) return;
    memcpy(path_buf, path, plen + 1u);

    DIR     dir;
    FILINFO fno;
    static char child[128];

    if (f_opendir(&dir, path_buf) != FR_OK) return;
    for (;;) {
        if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == '\0') break;
        size_t nlen = strlen(fno.fname);
        if (plen + 1u + nlen + 1u > sizeof(child)) continue;
        memcpy(child, path_buf, plen);
        child[plen] = '/';
        memcpy(child + plen + 1u, fno.fname, nlen + 1u);
        if (fno.fattrib & AM_DIR) rmdir_recursive(child);
        else                       f_unlink(child);
    }
    f_closedir(&dir);
    f_unlink(path_buf);
}

// ---------------------------------------------------------------------------
// API publique
// ---------------------------------------------------------------------------

bool ResInstaller_IsInstalled(void)
{
    uint32_t magic = 0;
    FlashMap_Read(RES_MAGIC_ADDR, (uint8_t *)&magic, 4u);
    return (magic == RES_MAGIC_VALUE);
}

bool ResInstaller_IsSlotValid(uint8_t slot)
{
    if (slot >= RES_IMG_MAX_SLOTS) return false;
    if (!ResInstaller_IsInstalled()) return false;
    uint8_t first = 0xFF;
    FlashMap_Read(RES_IMG_ADDR(slot), &first, 1u);
    return first != 0xFFu;
}

bool ResInstaller_GetSlotName(uint8_t slot, char *name_out)
{
    if (!ResInstaller_IsSlotValid(slot)) return false;
    FlashMap_Read(RES_IMG_ADDR(slot), (uint8_t *)name_out, RES_IMG_NAME_LEN);
    name_out[RES_IMG_NAME_LEN] = '\0';
    return true;
}

void ResInstaller_ShowResult(void)
{
    show_result();
}

bool ResInstaller_Run(void)
{
    if (f_mount(&s_fs, "0:", 1) != FR_OK) return false;

    FILINFO fno;
    if (f_stat("0:/res", &fno) != FR_OK) {
        f_mount(NULL, "0:", 0);
        return false;
    }

    GUI_Clear(BLACK);
    Font_DrawStringCentered(0, 10, LCD_WIDTH, 26, "INSTALLING RESOURCES", 1, 0x07FFu);

    // Érase du tampon magic (sécurité coupure de courant)
    FlashMap_EraseSector(RES_MAGIC_ADDR);

    // ---- Images --------------------------------------------------------
    collect_bmp_files();
    s_result_count = s_bmp_count;

    for (uint8_t i = 0; i < s_bmp_count; i++) {
        show_progress(i, s_bmp_count, s_bmp_files[i]);

        // Nom d'affichage (sans extension, tronqué à FNAME_DISP_LEN-1)
        uint8_t n = 0;
        for (; s_bmp_files[i][n] != '.' && s_bmp_files[i][n] != '\0'
               && n < FNAME_DISP_LEN - 1u; n++)
            s_results[i].fname[n] = s_bmp_files[i][n];
        s_results[i].fname[n] = '\0';

        erase_slot(i);
        s_results[i].status = install_slot(i, s_bmp_files[i]);
    }

    // ---- Sons (logique inchangée) --------------------------------------
    {
        DIR     dir_snd;
        FILINFO fno_snd;
        if (f_opendir(&dir_snd, "0:/res/sound") == FR_OK) {
            while (1) {
                if (f_readdir(&dir_snd, &fno_snd) != FR_OK || fno_snd.fname[0] == '\0') break;
                if (!is_snd_file(&fno_snd)) continue;

                char base[8];
                extract_base_name(fno_snd.fname, base);

                int8_t target    = -1;
                int8_t first_emp = -1;
                for (uint8_t s = 0; s < RES_SND_SLOT_COUNT; s++) {
                    if (slot_name_matches(s, base)) { target = (int8_t)s; break; }
                    if (first_emp < 0 && slot_is_empty(s)) first_emp = (int8_t)s;
                }
                if (target < 0) target = first_emp;
                if (target < 0) continue;
                if (RES_SND_ADDR((uint8_t)target) + RES_SND_SLOT_SIZE > SETTINGS_ADDR) continue;

                show_progress(0, 1, fno_snd.fname);   // indicateur simple pour les sons
                erase_snd_slot((uint8_t)target);
                install_snd_slot((uint8_t)target, fno_snd.fname);
            }
            f_closedir(&dir_snd);
        }
    }

    // ---- Magic (écrit en dernier pour la sécurité coupure) ------------
    uint32_t magic_val = RES_MAGIC_VALUE;
    FlashMap_Write(RES_MAGIC_ADDR, (uint8_t *)&magic_val, 4u);

    // ---- Renommage res/ → res_cur/ ------------------------------------
    FILINFO fno_cur;
    if (f_stat("0:/res_cur", &fno_cur) == FR_OK)
        rmdir_recursive("0:/res_cur");
    f_rename("0:/res", "0:/res_cur");

    f_mount(NULL, "0:", 0);
    return true;
}
