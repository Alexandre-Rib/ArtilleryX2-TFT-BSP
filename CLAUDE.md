# CLAUDE.md — Contexte projet ArtilleryX2-TFT-BSP

> Fichier de contexte pour Claude Code. Décrit le hardware, l'objectif, l'état
> du projet et les conventions.

---

## 1. Objectif du projet

Ce dépôt est un **fork** de `bigtreetech/BIGTREETECH-TouchScreenFirmware`, transformé
en **BSP (Board Support Package) minimal et polyvalent** pour la carte écran d'une
imprimante Artillery Sidewinder X2.

Le but est de fournir une base matérielle propre (init clocks, LCD, tactile, SD, USB,
flash externe) **sans aucun code spécifique imprimante 3D** (pas de Marlin,
pas de G-code, pas de menus d'imprimante).

### Feuille de route
1. ✅ BSP minimal compilant, affichage LCD, buzzer, clavier, joystick — périphériques validés. (Souris abandonnée : LCD_ReadPixels non fonctionnel sur HX8558 parallèle → curseur logiciel impossible.)
2. Portage de **Wolfenstein 3D** (raycasting, rendu colonne par colonne, pas de framebuffer plein écran).
3. Éventuellement : **émulateur Game Boy / NES** (ROMs sur SD ou clé USB).

---

## 2. Hardware cible

| Élément | Détail |
|---|---|
| Carte | Artillery "TFT Controller V1.0" (rebadge **MKS TFT28 V4.0**) |
| MCU | **STM32F107VC** (Cortex-M3, 72 MHz, pas de FPU) |
| Flash MCU | 256 KB (bootloader 0x0000–0x7000, applicatif à partir de 0x7000) |
| RAM | 64 KB |
| Contrôleur LCD | **HX8558** |
| Résolution LCD | 320 × 240 paysage fixe |
| Tactile | XPT2046 (SPI logiciel) |
| Flash externe | **Winbond W25Q64JV** — 8 Mo, SPI (puce U5) |
| Stockage amovible | Lecteur SD (SPI1) + USB Host OTG FS |
| Quartz | HSE déclaré 25 MHz dans platformio.ini (à vérifier sur hardware) |

---

## 3. Bootloader

- Version : **V3.0.0** (MKS/Artillery d'origine, intact, NON modifié).
- Mécanisme : lit la SD, cherche **`mkstft28.bin`** (minuscules) à la racine,
  flashe à **0x08007000**, renomme en `.cur`, reboot.
- Pas de JTAG/ST-Link : flash **uniquement via carte SD**.
- Filet de sécurité : firmware Artillery d'origine sur SD = restauration garantie.

---

## 4. Build (PlatformIO)

- Environnement : **`MKS_TFT28_V4_0`** — `board_build.filename = MKSTFT28`
- `framework = cmsis`, `board = STM32F107VC_0x7000`, `lib_ldf_mode = off`
- Le `.bin` produit → `out/MKS_TFT28_V4_0/release/MKSTFT28.bin` (prêt SD après renommage)

### auto_includes.py (script pre:)
- Scanne `lib/` et ajoute tous les dossiers contenant un `.h` comme `-I`
- `env.BuildSources(...)` compile tous les `.c`/`.S` de `lib/` → remplace le LDF
- **Aucun `-I` manuel** dans `platformio.ini` — tout est géré par ce script

### Flags clés
```
-DSYSTEM_LANGUAGE=ENGLISH    ← langue par défaut (configurable)
-DVECT_TAB_FLASH=0x08007000
-Wl,--allow-multiple-definition  ← conflit SystemInit avec framework CMSIS
```

---

## 5. Drivers BSP — notes importantes

### Delays
- `Delay_us` / `Delay_ms` : **bloquants** — polling SysTick, CPU suspendu
- `Buzzer_PlayTone` : bloquant (utilise `Delay_ms`)
- Non-bloquant périodique : `OS_GetTimeMs()` pour du time-based polling en boucle principale,
  ou hook dans `TIM7_IRQHandler` (1 ms), ou timer libre (TIM2/TIM3/TIM4/TIM6)
- Pour Wolf3D : son non bloquant via `TIM7` ou timer dédié à créer

### Buzzer
- Validé hardware : buzzer **passif** sur PA2, TIM5_CH3 PWM
- API : `Buzzer_Set(hz, volume)` non bloquant, `Buzzer_PlayTone(hz, volume, ms)` bloquant
- Volume 0–100 → duty cycle 0–50 % (50 % = excursion max = volume max)

### Clavier / Joystick USB HID
- Driver centralisé dans `keyboard.c/h` — encapsule la pile ST `STM32_USB_HOST_Library`
- **Deux defines obligatoires dans `platformio.ini`** :
  - `-DUSB_FLASH_DRIVE_SUPPORT` : active le code BSP réel dans `usb_bsp.c` et la définition de `USR_cb` dans `usbh_usr.c`.
  - `-DUSE_USB_OTG_FS` : définit `USB_OTG_FS_CORE` → active les tailles FIFO FS dans `usb_conf.h`.
- **Un seul périphérique USB à la fois** — les hubs ne sont pas supportés.
- Détection du type de périphérique par `HID_Machine.cb` :
  - `&HID_KEYBRD_cb` → clavier Boot Protocol → `Keyboard_IsConnected()`
  - `&HID_GENERIC_cb`→ joystick custom HID   → `Joystick_IsConnected()`
  - `&HID_MOUSE_cb`  → souris Boot Protocol (stub, non utilisé — voir note ci-dessous)
- **Clavier** : validé hardware. `Keyboard_GetKeycode()` lit `HID_Machine.buff[2]` (état instantané). Layouts QWERTY/AZERTY/QWERTZ via `Keyboard_SetLayout()`.
- **Joystick** : validé hardware via pont Arduino Nano → USB HID Generic. `Joystick_IsConnected()` + rapport HID traité par `HID_GENERIC_cb`.
- **Souris supprimée (BSP v1.2)** : `LCD_ReadPixels` non fiable sur le bus parallèle HX8558 (read-back timing marginal). `USR_MOUSE_Init/ProcessData` conservés comme stubs pour la pile USB ST, mais l'API Mouse_* est retirée.
- **Keyboard_Process()** : limité à 1 appel/ms + watchdog reconnexion automatique (2 s).
- **USB_GetDiagStr()** : retourne une chaîne compacte décrivant l'état HID brut (type cb, état machine, 2 premiers octets rapport).
- **Ne pas modifier `STM32_USB_HOST_Library/`** : passer par `keyboard.c` et les callbacks `USR_*`.

### Manette Mega Drive (`mega9.c/h`)
- La manette Sega Mega Drive 3 boutons est connectée via un **Arduino Nano** qui lit les
  signaux DB9 et les retransmet en **USB HID Generic** au STM32. L'Arduino agit comme
  pont transparent — le STM32 voit un gamepad HID, pas une manette DB9.
- Ce pont est un **side project dédié** :
  **https://github.com/Alexandre-Rib/mega3btn-to-usb-hid**
  Il contient le sketch Arduino + la bibliothèque de lecture manette Mega Drive utilisée.
- Format du rapport HID : Report ID `0x01`, 1 octet de boutons (masque `MEGA9_BTN_*`).
- **Mapping de navigation validé hardware** : **C = CONFIRM**, **A = BACK** (ne pas inverser).
- `Mega9_GetButtons()` : état instantané (masque MEGA9_BTN_*).
- `Mega9_GetNewButtons()` : fronts montants depuis le dernier appel — accumulés dans `Mega9_Process()`.
- `Mega9_Process()` doit être appelé à chaque itération de la boucle principale.
- `Mega9_FlushEdges()` : vider les fronts lors des transitions de scène.

### LCD_ReadPixels (`LCD_Init.c/h`)
- `LCD_ReadPixels(x, y, w, h, buf)` — lecture bulk GRAM RGB565.
- Protocole : `LCD_SetWindow()` (termine par cmd `0x2C`) → `LCD_WR_REG(0x2E)` (MIPI DCS Memory Read) → `Delay_us(5)` → dummy read → N lectures.
- `HX8558_ReadPixel_24Bit` dans `HX8558.c` utilise le même protocole pour la lecture d'un pixel unique.
- **Non validé sur hardware** (résultats non exploitables sur bus parallèle de cette carte — raison de l'abandon de la souris logicielle).

---

## 5. Architecture lib/

```
lib/
  bsp/                  Board Support Package
    mks_tft28.h         SEUL fichier board config (pins, LCD, UART, SD, USB)
    mks_tft28.c         MKS_TFT28_Init() — point d'entrée BSP
    delay.c/h           SysTick timer
    os_timer.c/h        TIM7 1ms — OS_GetTimeMs() utilisé par GUI et Serial
    stm32f10x/          CMSIS STM32F107 (stm32f10x.h, system_stm32f10x.c/h)
    fwlib/              Standard Peripheral Library STM32F10x (ST, ne pas modifier)
    Fatfs/              FatFS (diskio.c = glue BSP ; ff.c = middleware générique)
    drivers/            Drivers composants
      stm32f10x/        Drivers MCU : GPIO, SPI, UART, lcd, lcd_dma, timer_pwm, Serial, HAL_Flash
      LCD_Colors.c/h    Constantes couleur RGB565 (partagées BSP ↔ GUI)
      LCD_Init.c/h      Abstraction LCD (SetWindow, WR_16BITS_DATA, init séquence)
      LCD_Driver/       Driver chip HX8558 (seul driver LCD supporté)
      w25qxx.c/h        Driver W25Q64 flash externe
      xpt2046.c/h       Driver tactile XPT2046 (SW SPI, pins passés en paramètre)
      sd.c/h            Driver SD card (SPI)
      sw_spi.c/h        SPI logiciel générique
      keyboard.c/h      Driver USB HID : clavier + joystick (API publique BSP) — souris retirée BSP v1.2
      mega9.c/h         Driver manette Mega Drive via Arduino Nano (USB HID Generic)
      mouse_cursor.c/h  Fichier vide (supprimé — LCD_ReadPixels non fiable)
      STM32_USB_*/      Pile USB Host ST (ne pas modifier — sauf exception documentée)

  gui/                  API graphique
    GUI.c/h             Primitives : Clear, FillRect, DrawLine, DrawPixel…
    ui_draw.c/h         Rendu BMP/icônes depuis flash (utilise FlashMap_Read)
    font_render.c/h     Pixel width de chaînes (délègue à font_atlas)
    CharIcon.c/h        Icônes de caractères

  assets/               Ressources flash externe
    flash_map.h/c       API W25Q64 (FlashMap_Read/Write/EraseSector) + carte mémoire
                        Ré-exporte FLASH_PAGE_SIZE depuis w25qxx.h
    font_atlas.c/h      Résolution glyphes → adresse flash (dépend flash_map, utf8)
    Language/           Textes i18n multi-langues

src/demo/             Application de démonstration BSP
    demo_app.c/h        Boucle principale, machine d'états scènes, intégration LPC
    longpress_calib.c/h Trigger calibration par appui long 30 s (overlay, state machine)
    ui/
      ui_nav.c/h        Abstraction navigation (clavier, manette, souris, tactile)
      ui_menu.c/h       Gestion menus / focus / activation
      ui_button.c/h     Rendu boutons
    scenes/
      scene_calib.c/h   Calibration tactile (LIVE + PROCEDURE, lockout, débounce coins)
      scene_controllers.c/h  Sous-menu CTRL (clavier / souris / joystick)
      scene_keyboard.c/h     Diagnostic clavier HID
      scene_mouse.c/h        Diagnostic souris HID
      scene_joystick.c/h     Diagnostic manette Mega Drive
      scene_image/sound/anim Autres scènes de démonstration

  utils/                Utilitaires sans dépendance hardware
    utf8.c/h            Codec UTF-8 pur
    my_misc.c/h         Helpers math/CRC/string
    printf/             Printf léger (tiers)
    base64/             Base64 (tiers)
    json/               JSON (tiers)
    pngle/              PNG decode (tiers)
```

### Règles d'inclusion (sens autorisé : haut → bas)
- `gui/` peut inclure `assets/` et `bsp/drivers/` via leurs headers publics
- `assets/` peut inclure `bsp/drivers/`
- `bsp/drivers/` peut inclure `bsp/stm32f10x/` et `bsp/fwlib/`
- **Jamais** : driver → gui, bsp → assets, bsp → gui
- `flash_map.h` encapsule `w25qxx.h` — nul autre fichier hors `bsp/` n'inclut `w25qxx.h`
- `xpt2046.c` est générique : pins passés en paramètre par `mks_tft28.c`

---

## 6. État BSP (post sessions de nettoyage)

### Supprimé définitivement
- `includes.h` (umbrella mort) — 0 référence dans le projet
- `Configuration.h` — seul `SYSTEM_LANGUAGE` utilisé, déplacé dans `platformio.ini`
- `HW_Init.c/h` — fusionné dans `mks_tft28.c/h` comme `MKS_TFT28_Init()`
- `variants.h` + pin files + resolution files → `mks_tft28.h`
- `boot.c/h` → renommé `flash_map.c/h`
- Drivers morts : `ILI9341`, `sdio_sdcard`, `Knob_LED`, `HD44780`, `spi_slave`, `buzzer`
- `Hal/` → renommé `drivers/`
- `cmsis/stm32f10x/` → aplati en `stm32f10x/`

### Violations architecturales résolues
- `ui_draw.c` : accès SPI bruts W25Q64 → remplacés par `FlashMap_Read`
- `Language.h` : `w25qxx.h` → `flash_map.h` + `FLASH_PAGE_SIZE`
- `HX8558.c` : `LCD_SetWindow` circulaire → `HX8558_SetWindow` local
- `xpt2046.c` : import `mks_tft28.h` → pins passés en paramètre
- `LCD_Init.c` : `GUI_Clear` → clear natif LCD avec `BLACK` de `LCD_Colors.h`

### Violations restantes connues
- `GUI.c` → `lcd.h` direct (GUI → driver MCU, devrait passer par `LCD_Init.h`)
- `lcd_dma.c` → `w25qxx.h` (peer driver, acceptable)
- `os_timer.c` : fonctions `OS_Task*` jamais appelées (dead code partiel)
- `Language.h` → `mks_tft28.h` (assets → board config, discutable)

### Validé sur hardware ✅
- Carré vert affiché à l'écran — LCD HX8558 opérationnel
- Buzzer passif validé : fréquence ET volume fonctionnels (gamme + fade in/out confirmés)
- Clavier USB HID validé : déplacement d'un carré aux flèches, hold continu, stop au relâchement
- Joystick USB HID validé (via pont Arduino) : navigation directionnelle + boutons
- Manette Mega Drive validée : navigation directionnelle + C/A opérationnels
- ❌ LCD_ReadPixels non fonctionnel : read-back GRAM peu fiable sur bus parallèle de cette carte → souris logicielle abandonnée

### Navigation globale (`ui_nav.c`)
- **Priorité des sources** : clavier → manette → tactile
- **Clavier** : auto-repeat (400 ms délai, 120 ms cadence) sur toutes les directions + Entrée/Échap.
- **Manette** : C = CONFIRM, A = BACK (front montant, pas de repeat). Croix = UP/DOWN/LEFT/RIGHT avec auto-repeat identique au clavier.
- **Tactile** : front montant → `NAVIGATION_TOUCH` avec conversion ADC → pixels via calibration.

### Comportement des scènes de diagnostic — lockout navigation
- En mode diagnostic, le périphérique testé **ne peut pas se servir de lui-même pour sortir** si une calibration valide est en flash (`Settings_Load()` réussit).
  - Clavier → ESC ne sort pas du diagnostic clavier.
  - Manette → A ne sort pas du diagnostic joystick.
- Si **aucune calibration en flash** : le périphérique peut sortir (fallback — le tactile peut être inutilisable).
- `-DDIAG_BACK_UNLOCKED` dans `platformio.ini` désactive ce lockout entièrement.
- Vérification à la demande : `Settings_Load()` appelé directement dans chaque scène sur `NAVIGATION_BACK`, pas de flag en mémoire.

### Calibration par appui long (`longpress_calib.c/h`)
- Maintenir le tactile 30 s depuis n'importe quel menu → lance `SceneCalib_OnEnterProcedure()`.
- Compte à rebours 5→0 affiché à partir de 25 s (overlay centré par-dessus tout).
- À 30 s : message "Relâcher — attente 2 s". Puis lancement après 2 s de relâchement continu.
- `LongpressCalib_IsBlocking()` → `true` dès 25 s : les scènes sont gelées (overlay prioritaire).
- Retour à l'écran précédent via `restore_current_screen()` (appelle `on_enter` de la scène courante).

### Calibration tactile (`scene_calib.c`)
- **LIVE** : barres ADC brutes X/Y + MIN/MAX étiquetés "X MIN" / "X MAX" / "Y MIN" / "Y MAX" dans les bandes d'en-tête.
- **PROCEDURE** : 4 coins, résumé avec labels "X MIN" / "X MAX" / "Y MIN" / "Y MAX" à gauche de chaque valeur 7 segments.
- `SceneCalib_OnEnter()` → mode LIVE. `SceneCalib_OnEnterProcedure()` → mode PROCEDURE direct.
- Débounce entre coins : `CORNER_RELEASE_MS = 400 ms` de relâchement continu requis avant la prochaine capture (prévient le double-tap sur tactile résistif).
- **Pas de sauvegarde automatique** — bouton SAVE explicite uniquement.

### À faire
- Corriger `GUI.c` → `lcd.h` (retirer, tout passe par `LCD_Init.h`)
- Supprimer les fonctions `OS_Task*` mortes dans `os_timer.c`
- Vérifier fréquence HSE réelle (8 ou 25 MHz ?)
- Investiguer `LCD_ReadPixels` au scope/analyseur logique (timing RD# sur bus parallèle) — résoudre pour activer le save-under ou documenter définitivement comme non supporté

---

## 7. Conventions de travail

- **On ne commit QUE du code qui compile.**
- **Suppression physique** du code mort — pas de `#ifdef` pour désactiver.
- **Pas de `-I` manuels** dans `platformio.ini` → tout passe par `auto_includes.py`.
- **Pas de `library.json`** dans les sous-dossiers de `lib/`.
- L'utilisateur est **ingénieur systèmes embarqués** (STM32/ARM/C bas niveau, débutant PlatformIO).
  → Pas besoin de réexpliquer les bases ; être direct et concis.

### Remote utile
```
upstream = https://github.com/bigtreetech/BIGTREETECH-TouchScreenFirmware.git
```

---

## 8. État des branches et tags

| Tag / Branche | Description |
|---|---|
| `BSP_V1_0_0` | BSP minimal : LCD, buzzer, buzzer |
| `BSP_V1_1_0` | Ajout clavier USB HID |
| `BSP_V1_1_1` | Housekeeping — commentaires anglais, headers |
| `DEMO_V1_0_0` | Branche `demo/bsp-showcase` — démo BSP complète (menus, calib, scènes) |
| `update/BSP_V1_2_x_update` | Porting BSP depuis démo : joystick, watchdog USB, LCD_ReadPixels |

### Démo BSP (`demo/bsp-showcase`) — état DEMO_V1_0_0
- Menu principal **2×2** : IMAGE / SOUND / CALIB / CTRL
- Sous-menu CTRL : KEYBOARD + JOYSTICK (souris supprimée)
- Scènes : `scene_anim.c/h` et `scene_mouse.c/h` vidés (hors scope)
- `mouse_cursor.c/h` vidés (LCD_ReadPixels non fiable)

---

## 9. Notes Wolfenstein (pour plus tard)

- Rendu en **colonnes verticales** → SetWindow + envoi série de pixels. Pas de framebuffer plein écran.
- Palette VGA 256 couleurs → LUT RGB565 précalculée en flash.
- Assets (~750 Ko shareware) → flash externe W25Q64JV (8 Mo largement suffisant).
- Pas de FPU → fixed-point (Wolf3D était déjà en fixed-point d'origine).
- Framerate visé : 15–30 fps.

## 9. Notes émulateur (plus tard)

- ROMs sur SD (FatFS déjà présent) ou clé USB (USB Host).
- GB ROM : 32 Ko–8 Mo. NES ROM : 8–512 Ko.
