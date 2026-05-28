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
| Écran | **HX8558**, 320 × 240, interface SPI/FSMC |
| Tactile | XPT2046, SPI |
| Flash externe | **Winbond W25Q64JV**, 8 Mo (polices, assets) |
| Stockage | Lecteur SD + port USB-A (USB Host) |
| Bootloader | MKS/Artillery V3.0.0 — flash via carte SD (`mkstft28.bin`) |

## Ce que le BSP fournit

- Initialisation des horloges (72 MHz, PLL via HSE 25 MHz)
- Pilote LCD HX8558 complet (init, SetWindow, DMA)
- API graphique : `GUI_Clear`, `GUI_FillRect`, `GUI_DrawLine`, `GUI_DrawPixel`, etc.
- Pilote tactile XPT2046
- Pilote flash externe W25Q64 (lecture / écriture / effacement)
- FatFS (accès SD)
- USB Host (MSC — clés USB)
- Timers : SysTick (Delay_us/ms), TIM7 (OS_Timer 1 ms)
- UART / Serial (DMA)
- Chargement d'assets BMP depuis SD vers flash externe

## Build

```
Plateforme  : PlatformIO, ststm32
Framework   : CMSIS
Environnement : MKS_TFT28_V4_0
```

```bash
pio run -e MKS_TFT28_V4_0
```

Le `.bin` produit est copié sous `mkstft28.bin` à la racine de la carte SD.
Au démarrage, le bootloader le flashe à `0x08007000` puis démarre l'application.

## Structure

```
src/          — main.c (point d'entrée)
lib/
  API/        — GUI, LCD_Colors, HW_Init, boot (asset loader)
  Hal/        — Drivers hardware : LCD, SPI, UART, USB, W25Qxx, XPT2046
  Fatfs/      — FatFS standard
  Variants/   — Configuration pinout MKS_TFT28
  cmsis/      — CMSIS STM32F10x
  fwlib/      — Standard Peripheral Library STM32F10x
scripts/      — auto_includes.py, IAP, deploy
ldscripts/    — Script linker (offset 0x08007000)
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
| Display | **HX8558**, 320 × 240, SPI/FSMC interface |
| Touch | XPT2046, SPI |
| External Flash | **Winbond W25Q64JV**, 8 MB (fonts, game assets) |
| Storage | SD card reader + USB-A port (USB Host) |
| Bootloader | MKS/Artillery V3.0.0 — flashed via SD card (`mkstft28.bin`) |

## What the BSP provides

- Clock initialisation (72 MHz, PLL via 25 MHz HSE)
- Full HX8558 LCD driver (init, SetWindow, DMA transfer)
- Graphics API: `GUI_Clear`, `GUI_FillRect`, `GUI_DrawLine`, `GUI_DrawPixel`, etc.
- XPT2046 touch driver
- W25Q64 external flash driver (read / write / erase)
- FatFS (SD card access)
- USB Host (MSC — USB flash drives)
- Timers: SysTick (Delay_us/ms), TIM7 (1 ms OS_Timer)
- UART / Serial with DMA
- BMP asset loader (SD → external flash)

## Building

```
Platform    : PlatformIO, ststm32
Framework   : CMSIS
Environment : MKS_TFT28_V4_0
```

```bash
pio run -e MKS_TFT28_V4_0
```

The produced `.bin` is renamed `mkstft28.bin` and placed at the SD card root.
On power-up, the bootloader flashes it to `0x08007000` and starts the application.

## Repository layout

```
src/          — main.c (entry point)
lib/
  API/        — GUI, LCD_Colors, HW_Init, boot (asset loader)
  Hal/        — Hardware drivers: LCD, SPI, UART, USB, W25Qxx, XPT2046
  Fatfs/      — Standard FatFS
  Variants/   — MKS_TFT28 pin configuration
  cmsis/      — CMSIS STM32F10x
  fwlib/      — STM32F10x Standard Peripheral Library
scripts/      — auto_includes.py, IAP, deploy helpers
ldscripts/    — Linker script (0x08007000 offset)
```

## Upstream project

Forked from **BIGTREETECH-TouchScreenFirmware**
([github.com/bigtreetech/BIGTREETECH-TouchScreenFirmware](https://github.com/bigtreetech/BIGTREETECH-TouchScreenFirmware))
— a full-featured TFT firmware for 3D printer controller screens.
