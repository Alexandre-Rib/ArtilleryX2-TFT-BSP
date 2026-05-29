> **English below**

---

# ArtilleryX2-TFT-BSP

**BSP (Board Support Package) minimal pour la carte écran Artillery Sidewinder X2**

## Objectif

Ce dépôt est un **fork épuré** de [BIGTREETECH-TouchScreenFirmware](https://github.com/bigtreetech/BIGTREETECH-TouchScreenFirmware).

L'objectif est de fournir une **base matérielle propre et générique** pour la carte écran
de l'imprimante Artillery Sidewinder X2, **sans aucun code spécifique imprimante 3D**.
Ni Marlin, ni G-code, ni menus d'impression.

Ce BSP est conçu pour servir de fondation à des projets embarqués indépendants sur cette
carte, par exemple :

- **Portage de Wolfenstein 3D** — raycasting colonne par colonne, adapté à la résolution 320×240
- **Émulateur NES** — ROMs sur carte SD ou clé USB
- **Émulateur Game Boy** — développement possible sur la même base hardware

## Hardware

| Élément | Détail |
|---|---|
| Carte | Artillery "TFT Controller V1.0" (rebadge **MKS TFT28 V4.0**) |
| MCU | **STM32F107VC** — Cortex-M3, 72 MHz, 64 KB RAM, 256 KB Flash |
| Écran | **HX8558**, 320 × 240, interface parallèle 16 bits |
| Tactile | XPT2046, SPI logiciel |
| Flash externe | **Winbond W25Q64JV**, 8 Mo (polices, assets) |
| Stockage | Lecteur SD + port USB-A (USB Host) |
| Bootloader | MKS/Artillery V3.0.0 — flash via carte SD (`mkstft28.bin`) |

## Ce que le BSP fournit

- Initialisation des horloges (72 MHz, PLL via HSE 25 MHz)
- Pilote LCD HX8558 complet (init, SetWindow)
- API graphique : `GUI_Clear`, `GUI_FillRect`, `GUI_DrawLine`, `GUI_DrawPixel`, etc.
- Pilote tactile XPT2046
- Pilote flash externe W25Q64 (lecture / écriture / effacement)
- FatFS (accès SD)
- USB Host (MSC — clés USB)
- Timers : SysTick (Delay_us/ms), TIM7 (OS_Timer 1 ms)
- UART / Serial (DMA)
- Chargement d'assets BMP depuis SD vers flash externe
- **Buzzer PWM** — pilote TIM5_CH3 sur PA2 avec contrôle de fréquence et de volume
- **LCD burst fill** — `LCD_FillColor(color, count)` : CS/RS positionnés une seule fois pour N pixels, 3 ops GPIO/pixel au lieu de 6 (~2× plus rapide sur les fills)

## Pré-requis

### 1. VS Code + extension PlatformIO

Télécharger [VS Code](https://code.visualstudio.com), puis installer l'extension
**PlatformIO IDE** depuis le marketplace (rechercher « PlatformIO »).

C'est **le seul outil à installer manuellement**. PlatformIO télécharge ensuite
automatiquement au premier build :
- La toolchain ARM (`arm-none-eabi-gcc`)
- La plateforme `ststm32`
- Le framework CMSIS

> Pas besoin d'installer `arm-none-eabi-gcc` séparément ni aucun autre outil.

### 2. Cloner le dépôt

```bash
git clone https://github.com/<user>/ArtilleryX2-TFT-BSP.git
cd ArtilleryX2-TFT-BSP
```

## Compiler

### Via VS Code

1. Ouvrir le dossier dans VS Code
2. Cliquer sur l'icône PlatformIO dans la barre latérale
3. Choisir **Build** (ou `Ctrl+Alt+B`)

### Via le terminal

```bash
pio run -e MKS_TFT28_V4_0
```

### Ce que fait le build (pipeline détaillée)

```
1. pre: auto_includes.py
   ├── Scanne lib/ et ajoute tous les dossiers contenant un .h comme -I
   └── Force la compilation de tous les .c de lib/ (remplace le LDF désactivé)

2. Compilation / Link  (arm-none-eabi-gcc)
   └── stm32f107xC_0x7000_iap.py remplace le linker script → offset 0x08007000

3. post: deploy_assets.py
   ├── Copie le .bin sous out/MKS_TFT28_V4_0/release/MKSTFT28.bin
   ├── Copie res/pic/   → out/MKS_TFT28_V4_0/release/pic/   (si le dossier existe)
   └── Copie res/sound/ → out/MKS_TFT28_V4_0/release/sound/ (si le dossier existe)
```

Le dossier `out/MKS_TFT28_V4_0/release/` contient tout ce qui va sur la carte SD.

### Ajouter des assets graphiques (icônes BMP)

Les assets sont des images BMP chargées dans la flash externe W25Q64 au premier démarrage.
Pour les inclure dans la release :

```
res/
  pic/       ← images BMP (icônes, logo…)
  sound/     ← fichiers son (non utilisés dans ce BSP)
```

Créer ces dossiers à la racine du projet, y placer les fichiers.
Après le build, ils apparaissent automatiquement dans `out/MKS_TFT28_V4_0/release/`.

## Driver Buzzer

> Driver généré par [Claude](https://claude.ai) (Anthropic) — testé et validé sur hardware.

La carte embarque un **buzzer passif** sur **PA2**, piloté en **PWM matériel via TIM5_CH3**.
Contrairement à un simple toggle GPIO, cette approche permet de contrôler à la fois la
**fréquence** (note musicale) et le **volume** (duty cycle), sans charge CPU.

### API

```c
#include "buzzer.h"

Buzzer_PlayTone(uint16_t hz, uint8_t volume, uint16_t ms);  // bloquant
Buzzer_Set(uint16_t hz, uint8_t volume);                    // non bloquant
Buzzer_Stop(void);
Buzzer_Beep(void);                                          // bip UI 2 kHz / 50 ms
```

- `volume` : 0 (silence) → 100 (maximum). Traduit en duty cycle 0–50 %.
- Plage de fréquences : ~30 Hz à 20 kHz.

### Exemple validé — gamme Do–Do

```c
#include "buzzer.h"

// Gamme ascendante à volume 20 (discret)
Buzzer_PlayTone(262, 20, 200);   // Do
Buzzer_PlayTone(294, 20, 200);   // Ré
Buzzer_PlayTone(330, 20, 200);   // Mi
Buzzer_PlayTone(349, 20, 200);   // Fa
Buzzer_PlayTone(392, 20, 200);   // Sol
Buzzer_PlayTone(440, 20, 200);   // La
Buzzer_PlayTone(494, 20, 200);   // Si
Buzzer_PlayTone(523, 20, 300);   // Do (octave)
```

## Flasher sur la carte

Pas de ST-Link — le bootloader Artillery gère le flash via carte SD :

1. Copier `MKSTFT28.bin` à la racine de la carte SD
2. Renommer le fichier en **`mkstft28.bin`** (minuscules obligatoires)
3. Insérer la carte SD dans la TFT et mettre sous tension
4. Le bootloader détecte le fichier, le flashe à `0x08007000`, le renomme en `.cur` puis redémarre

> Si le firmware est cassé, remettre l'original Artillery sur la SD pour restaurer.

## Configuration

Les features du BSP sont activées par défaut. Elles peuvent être désactivées ou
reconfigurées dans `platformio.ini` via `build_flags`.

| Feature | Défaut | Pour désactiver |
|---|---|---|
| Bus LCD 16 bits | `LCD_DATA_16BIT=1` | `-DLCD_DATA_16BIT=0` |
| Support carte SD | `SD_SPI_SUPPORT` défini | Ne pas définir + retirer `sd.c` / FatFS du build |
| Support USB Host | `USB_FLASH_DRIVE_SUPPORT` défini | Ne pas définir + retirer `STM32_USB_HOST_Library/` |
| Port série principal | `SERIAL_PORT=_USART2` | `-DSERIAL_PORT=_USART1` (ou autre) |

> **Aucun `build_flags` de feature n'est nécessaire** pour la configuration
> complète par défaut — `mks_tft28.h` contient toutes les valeurs de la carte.

Référence complète des options : voir [`lib/bsp/mks_tft28.h`](lib/bsp/mks_tft28.h)

## Structure du projet

```
src/              — main.c (point d'entrée)
lib/
  bsp/            — Hardware pur : init, timers, CMSIS, fwlib, FatFS
    drivers/      — Drivers composants : LCD HX8558, XPT2046, W25Qxx, SPI, UART, USB
    cmsis/        — CMSIS STM32F10x
    fwlib/        — Standard Peripheral Library STM32F10x
    Fatfs/        — FatFS
  gui/            — API graphique : GUI, LCD_Colors, ui_draw, font_render
  assets/         — Ressources flash : flash_map, font_atlas, Language
  utils/          — Utilitaires génériques : utf8, my_misc, printf, base64
boards/           — Définition board PlatformIO (STM32F107VC_0x7000)
ldscripts/        — Script linker (offset 0x08007000 pour le bootloader)
scripts/          — Scripts de build PlatformIO (includes auto, IAP, deploy)
out/              — Binaires de release (généré au build)
```

## Projet d'origine

Fork de **BIGTREETECH-TouchScreenFirmware**
([github.com/bigtreetech/BIGTREETECH-TouchScreenFirmware](https://github.com/bigtreetech/BIGTREETECH-TouchScreenFirmware))
— firmware complet pour écrans TFT de contrôleurs d'imprimantes 3D.

---

---

# ArtilleryX2-TFT-BSP

**Minimal BSP (Board Support Package) for the Artillery Sidewinder X2 TFT screen board**

## Purpose

This repository is a **stripped fork** of [BIGTREETECH-TouchScreenFirmware](https://github.com/bigtreetech/BIGTREETECH-TouchScreenFirmware).

The goal is to provide a **clean, generic hardware base** for the Artillery Sidewinder X2
printer's TFT screen board, **with all 3D printer-specific code removed**.
No Marlin, no G-code, no print menus.

This BSP is designed as a foundation for standalone embedded projects on this board, such as:

- **Wolfenstein 3D port** — column-by-column raycasting, fits the 320×240 display perfectly
- **NES emulator** — ROMs loaded from SD card or USB flash drive
- **Game Boy emulator** — feasible on the same hardware base

## Hardware

| Component | Detail |
|---|---|
| Board | Artillery "TFT Controller V1.0" (rebadged **MKS TFT28 V4.0**) |
| MCU | **STM32F107VC** — Cortex-M3, 72 MHz, 64 KB RAM, 256 KB Flash |
| Display | **HX8558**, 320 × 240, 16-bit parallel interface |
| Touch | XPT2046, software SPI |
| External Flash | **Winbond W25Q64JV**, 8 MB (fonts, game assets) |
| Storage | SD card reader + USB-A port (USB Host) |
| Bootloader | MKS/Artillery V3.0.0 — flashed via SD card (`mkstft28.bin`) |

## What the BSP provides

- Clock initialisation (72 MHz, PLL via 25 MHz HSE)
- Full HX8558 LCD driver (init, SetWindow)
- Graphics API: `GUI_Clear`, `GUI_FillRect`, `GUI_DrawLine`, `GUI_DrawPixel`, etc.
- XPT2046 touch driver
- W25Q64 external flash driver (read / write / erase)
- FatFS (SD card access)
- USB Host (MSC — USB flash drives)
- Timers: SysTick (Delay_us/ms), TIM7 (1 ms OS_Timer)
- UART / Serial with DMA
- BMP asset loader (SD → external flash)
- **Buzzer PWM** — TIM5_CH3 driver on PA2 with frequency and volume control
- **LCD burst fill** — `LCD_FillColor(color, count)`: CS/RS set once for N pixels, 3 GPIO ops/pixel instead of 6 (~2× faster fills)

## Prerequisites

### 1. VS Code + PlatformIO extension

Download [VS Code](https://code.visualstudio.com), then install the
**PlatformIO IDE** extension from the marketplace (search for "PlatformIO").

That is **the only tool you need to install manually**. On the first build,
PlatformIO automatically downloads:
- The ARM toolchain (`arm-none-eabi-gcc`)
- The `ststm32` platform
- The CMSIS framework

> No need to install `arm-none-eabi-gcc` separately or any other tool.

### 2. Clone the repository

```bash
git clone https://github.com/<user>/ArtilleryX2-TFT-BSP.git
cd ArtilleryX2-TFT-BSP
```

## Building

### From VS Code

1. Open the folder in VS Code
2. Click the PlatformIO icon in the sidebar
3. Click **Build** (or `Ctrl+Alt+B`)

### From the terminal

```bash
pio run -e MKS_TFT28_V4_0
```

### Build pipeline

```
1. pre: auto_includes.py
   ├── Scans lib/ and adds every directory containing a .h as an -I flag
   └── Forces compilation of all .c files in lib/ (replaces the disabled LDF)

2. Compile / Link  (arm-none-eabi-gcc)
   └── stm32f107xC_0x7000_iap.py rewrites the linker script → 0x08007000 offset

3. post: deploy_assets.py
   ├── Copies the .bin to out/MKS_TFT28_V4_0/release/MKSTFT28.bin
   ├── Copies res/pic/   → out/MKS_TFT28_V4_0/release/pic/   (if the folder exists)
   └── Copies res/sound/ → out/MKS_TFT28_V4_0/release/sound/ (if the folder exists)
```

The `out/MKS_TFT28_V4_0/release/` folder contains everything that goes on the SD card.

### Adding graphical assets (BMP icons)

Assets are BMP images loaded into external W25Q64 flash on first boot.
To include them in the release:

```
res/
  pic/       ← BMP images (icons, logo…)
  sound/     ← sound files (unused in this BSP)
```

Create these folders at the project root and place your files there.
After each build they are automatically copied to `out/MKS_TFT28_V4_0/release/`.

## Buzzer driver

> Driver written by [Claude](https://claude.ai) (Anthropic) — tested and validated on hardware.

The board includes a **passive buzzer** on **PA2**, driven by **hardware PWM via TIM5_CH3**.
Unlike a simple GPIO toggle, this approach gives independent control over both **frequency**
(musical note) and **volume** (duty cycle), with zero CPU overhead during playback.

### API

```c
#include "buzzer.h"

Buzzer_PlayTone(uint16_t hz, uint8_t volume, uint16_t ms);  // blocking
Buzzer_Set(uint16_t hz, uint8_t volume);                    // non-blocking
Buzzer_Stop(void);
Buzzer_Beep(void);                                          // UI beep 2 kHz / 50 ms
```

- `volume`: 0 (silent) → 100 (maximum). Mapped internally to 0–50 % duty cycle.
- Frequency range: ~30 Hz to 20 kHz.

### Validated example — C major scale

```c
#include "buzzer.h"

// Ascending scale at volume 20 (quiet)
Buzzer_PlayTone(262, 20, 200);   // C
Buzzer_PlayTone(294, 20, 200);   // D
Buzzer_PlayTone(330, 20, 200);   // E
Buzzer_PlayTone(349, 20, 200);   // F
Buzzer_PlayTone(392, 20, 200);   // G
Buzzer_PlayTone(440, 20, 200);   // A
Buzzer_PlayTone(494, 20, 200);   // B
Buzzer_PlayTone(523, 20, 300);   // C (octave)
```

## Flashing

No ST-Link required — the Artillery bootloader handles flashing via SD card:

1. Copy `MKSTFT28.bin` to the root of the SD card
2. Rename it to **`mkstft28.bin`** (lowercase required)
3. Insert the SD card into the TFT board and power on
4. The bootloader detects the file, flashes it to `0x08007000`, renames it to `.cur`, then reboots

> If the firmware is broken, put the original Artillery firmware on the SD card to restore.

## Configuration

BSP features are enabled by default. They can be disabled or reconfigured
in `platformio.ini` via `build_flags`.

| Feature | Default | To disable |
|---|---|---|
| 16-bit LCD bus | `LCD_DATA_16BIT=1` | `-DLCD_DATA_16BIT=0` |
| SD card support | `SD_SPI_SUPPORT` defined | Leave undefined + remove `sd.c` / FatFS from build |
| USB Host support | `USB_FLASH_DRIVE_SUPPORT` defined | Leave undefined + remove `STM32_USB_HOST_Library/` |
| Main serial port | `SERIAL_PORT=_USART2` | `-DSERIAL_PORT=_USART1` (or other) |

> **No feature `build_flags` are needed** for the default full configuration —
> `mks_tft28.h` already contains all board values.

Full options reference: [mks_tft28_features.md](mks_tft28_features.md)

## Repository layout

```
src/              — main.c (entry point)
lib/
  bsp/            — Pure hardware: init, timers, CMSIS, fwlib, FatFS
    drivers/      — Component drivers: LCD HX8558, XPT2046, W25Qxx, SPI, UART, USB
    cmsis/        — CMSIS STM32F10x
    fwlib/        — STM32F10x Standard Peripheral Library
    Fatfs/        — FatFS
  gui/            — Graphics API: GUI, LCD_Colors, ui_draw, font_render
  assets/         — Flash resources: flash_map, font_atlas, Language
  utils/          — Generic utilities: utf8, my_misc, printf, base64
boards/           — PlatformIO board definition (STM32F107VC_0x7000)
ldscripts/        — Linker script (0x08007000 offset for the bootloader)
scripts/          — PlatformIO build scripts (auto-includes, IAP, deploy)
out/              — Release binaries (generated at build time)
```

## Upstream project

Forked from **BIGTREETECH-TouchScreenFirmware**
([github.com/bigtreetech/BIGTREETECH-TouchScreenFirmware](https://github.com/bigtreetech/BIGTREETECH-TouchScreenFirmware))
— a full-featured TFT firmware for 3D printer controller screens.
