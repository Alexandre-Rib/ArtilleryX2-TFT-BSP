> **English below**

---

# ArtilleryX2-TFT-BSP — Application de démonstration

Firmware de démonstration pour la carte écran de l'**Artillery Sidewinder X2**
(rebadge MKS TFT28 V4.0 — MCU STM32F107VC, écran HX8558 320×240, tactile XPT2046).

---

## Flasher le firmware

Pas de ST-Link. Le bootloader Artillery flash via carte SD :

1. Copier `MKSTFT28.bin` à la racine de la SD
2. Renommer en **`mkstft28.bin`** (minuscules obligatoires)
3. Insérer la SD et mettre sous tension
4. Le bootloader flashe à `0x08007000`, renomme en `.cur` puis redémarre

> Pour restaurer : remettre le firmware Artillery d'origine sur la SD.

---

## Compiler

```bash
pio run -e MKS_TFT28_V4_0
```

Le binaire produit est `out/MKS_TFT28_V4_0/release/MKSTFT28.bin`.

---

## Navigation

Trois modes d'entrée, interchangeables à tout moment :

| Entrée | Déplacement | Valider | Retour |
|---|---|---|---|
| Clavier USB | Flèches | Entrée | Échap |
| Manette Mega Drive | Croix | **C** | **A** |
| Tactile | Tap | Tap | Bouton BACK à l'écran |

> **Limitation USB :** un seul périphérique à la fois — clavier **ou** souris **ou** manette.
> Les hubs USB ne sont **pas** supportés.

---

## Menu principal

Grille 3 × 2. Le focus suit la navigation clavier/manette (anneau cyan).

```
┌──────────┬──────────┬──────────┐
│  IMAGE   │  SOUND   │   ANIM   │
├──────────┼──────────┼──────────┤
│  CALIB   │   CTRL   │  (vide)  │
└──────────┴──────────┴──────────┘
```

---

## IMAGE

Visionneur de fichiers BMP depuis la carte SD. Navigation dans l'arborescence,
affichage plein écran avec recadrage automatique.

### Formats acceptés (carte SD)

| Format | Détail |
|---|---|
| BMP 24 bits | Sans compression (le plus courant) |
| BMP 16 bits | RGB565, compression type 3 (masques R=0xF800 / G=0x07E0 / B=0x001F) |

> **PNG non supporté.** La bibliothèque `pngle` est présente dans `lib/utils/` mais
> non connectée au viewer. Seul le BMP est lu.

---

## SOUND

Démonstration du buzzer passif (PA2 / TIM5 CH3).
Joue des séquences de notes. Contrôle du volume global persisté en flash externe.

---

## ANIM

Animation graphique de démonstration — test du rendu LCD et des primitives GUI.

---

## CALIB — Calibration du tactile

### Mode LIVE

Affiche les valeurs ADC brutes des axes X et Y en temps réel :

- Barre de progression (valeur courante)
- **X MIN / X MAX** : plage de variation observée sur l'axe X
- **Y MIN / Y MAX** : plage de variation observée sur l'axe Y
- Réticule calé sur la calibration active

### Mode PROCEDURE

Guidage 4 coins (Haut-Gauche → Haut-Droit → Bas-Droit → Bas-Gauche).
Appuyer sur chaque coin quand il clignote. Une fois les 4 coins capturés :

- Le résumé affiche les 4 valeurs avec leur label (**X MIN**, **X MAX**, **Y MIN**, **Y MAX**)
- Le bouton **SAVE** s'active pour persister en flash externe

**La sauvegarde n'est jamais automatique** — elle nécessite un appui explicite sur SAVE.

### Calibration par appui long (depuis n'importe quel écran)

Maintenir le doigt sur l'écran tactile pendant 30 secondes lance la procédure
sans passer par les menus.

| Durée de l'appui | Affichage |
|---|---|
| 0 – 25 s | Rien (navigation normale) |
| 25 – 30 s | Compte à rebours **5 → 4 → 3 → 2 → 1** superposé à l'écran |
| ≥ 30 s | « Relâcher l'écran — attente 2 s après relâchement » |
| Après relâchement | Compte à rebours **2 s → 1 s**, puis lancement direct de la procédure 4 coins |

Appuyer sur BACK/QUIT dans la procédure ramène à l'écran précédent.
La sauvegarde reste explicite (bouton SAVE).

---

## CTRL — Périphériques USB

Sous-menu à 3 tuiles. Une tuile est grisée si son périphérique est débranché.
Déconnexion en cours de session → retour automatique au sous-menu.

> **Un seul périphérique USB à la fois.** Les hubs ne sont pas supportés.

### KEYBOARD — Clavier USB HID

Affiche en temps réel :
- **KEYCODE** : code HID brut (hex)
- **MOD** : octet de modificateurs — Shift, Ctrl, Alt… (hex)
- **CHAR** : caractère ASCII décodé

Log des 7 dernières frappes avec défilement automatique.

Layouts supportés : QWERTY (US) · AZERTY (FR) · QWERTZ (DE)

> **Touche Échap :** en mode diagnostic clavier, Échap n'entraîne **pas** de sortie
> si une calibration est enregistrée en flash (utiliser le bouton BACK à l'écran).
> Si aucune calibration n'existe, Échap sert de sortie de secours.

### MOUSE — Souris USB HID

- Position X / Y du curseur (delta, indépendant de la calibration tactile)
- État des boutons gauche / droit (PRESSED / released)
- Curseur graphique visible sur tout l'écran

Sortie : clic gauche sur le bouton BACK en bas de l'écran.

### JOYSTICK — Manette Sega Mega Drive

Affiche l'état des 8 boutons en temps réel : UP / DOWN / LEFT / RIGHT / A / B / C / START.

Mapping de navigation depuis les autres menus :
- **C** = Valider
- **A** = Retour
- Croix directionnelle = navigation dans les menus (avec auto-répétition)

> **Bouton A en mode diagnostic :** comme pour le clavier, A n'entraîne **pas** de sortie
> si une calibration est valide. Utiliser le bouton BACK à l'écran.

---

## Assets graphiques — format des icônes

Les icônes des tuiles de menu sont installées depuis `res/pic/` (carte SD) vers la
flash externe (W25Q64) au premier démarrage.

| Paramètre | Valeur |
|---|---|
| Taille | **80 × 80 pixels exactement** |
| Format source | **BMP 24 bits sans compression** |
| Stockage flash | Converti en RGB565 (2 octets/pixel — 12 800 octets/icône) |
| PNG | **Non supporté** |

Une image de taille incorrecte sera affichée tronquée ou avec des artefacts.

Pour ajouter des icônes : placer les fichiers dans `res/pic/` à la racine du projet.
Le script de build les copie automatiquement dans `out/.../release/pic/`.

---

---

# ArtilleryX2-TFT-BSP — Demo application

Demo firmware for the **Artillery Sidewinder X2** TFT screen board
(MKS TFT28 V4.0 rebadge — MCU STM32F107VC, HX8558 320×240 display, XPT2046 touch).

---

## Flashing

No ST-Link required. The Artillery bootloader flashes via SD card:

1. Copy `MKSTFT28.bin` to the SD card root
2. Rename to **`mkstft28.bin`** (lowercase required)
3. Insert the SD card and power on
4. The bootloader flashes to `0x08007000`, renames to `.cur`, then reboots

> To restore: put the original Artillery firmware on the SD card.

---

## Building

```bash
pio run -e MKS_TFT28_V4_0
```

Output binary: `out/MKS_TFT28_V4_0/release/MKSTFT28.bin`.

---

## Navigation

Three input methods, usable at any time:

| Input | Move | Confirm | Back |
|---|---|---|---|
| USB keyboard | Arrow keys | Enter | Escape |
| Mega Drive gamepad | D-pad | **C** | **A** |
| Touchscreen | Tap | Tap | On-screen BACK button |

> **USB limitation:** only one device at a time — keyboard **or** mouse **or** gamepad.
> USB hubs are **not** supported.

---

## Main menu

3 × 2 grid. Focus follows keyboard/gamepad navigation (cyan ring).

```
┌──────────┬──────────┬──────────┐
│  IMAGE   │  SOUND   │   ANIM   │
├──────────┼──────────┼──────────┤
│  CALIB   │   CTRL   │ (empty)  │
└──────────┴──────────┴──────────┘
```

---

## IMAGE

BMP file viewer from SD card. Browse the directory tree, display full-screen
with automatic cropping.

### Accepted formats (SD card)

| Format | Details |
|---|---|
| 24-bit BMP | No compression (most common) |
| 16-bit BMP | RGB565, compression type 3 (masks R=0xF800 / G=0x07E0 / B=0x001F) |

> **PNG not supported.** The `pngle` library exists in `lib/utils/` but is not wired
> into the viewer. BMP only.

---

## SOUND

Passive buzzer demo (PA2 / TIM5 CH3).
Plays note sequences. Global volume control persisted to external flash.

---

## ANIM

Graphics animation demo — LCD rendering and GUI primitives test.

---

## CALIB — Touch calibration

### LIVE mode

Displays raw ADC values for both axes in real time:

- Progress bar (current value)
- **X MIN / X MAX**: observed ADC range on the X axis
- **Y MIN / Y MAX**: observed ADC range on the Y axis
- Crosshair mapped through the active calibration

### PROCEDURE mode

Guided 4-corner calibration (Top-Left → Top-Right → Bottom-Right → Bottom-Left).
Tap each corner when it blinks. Once all 4 corners are captured:

- The summary shows all 4 values with their labels (**X MIN**, **X MAX**, **Y MIN**, **Y MAX**)
- The **SAVE** button becomes active to persist to external flash

**Nothing is saved automatically** — an explicit SAVE button press is required.

### Long-press calibration trigger (from any screen)

Hold a finger on the touchscreen for 30 seconds to launch the procedure
without going through the menus.

| Press duration | Display |
|---|---|
| 0 – 25 s | Nothing (normal navigation) |
| 25 – 30 s | Countdown **5 → 4 → 3 → 2 → 1** overlaid on the current screen |
| ≥ 30 s | "Release screen — waiting 2 s after release" |
| After release | **2 s → 1 s** countdown, then 4-corner procedure launches directly |

Pressing BACK/QUIT in the procedure returns to the previous screen.
Saving remains explicit (SAVE button).

---

## CTRL — USB peripherals

Sub-menu with 3 tiles. A tile is greyed out when its device is unplugged.
Unplugging during a session → automatic return to the sub-menu.

> **One USB device at a time.** Hubs are not supported.

### KEYBOARD — USB HID keyboard

Real-time display of:
- **KEYCODE**: raw HID code (hex)
- **MOD**: modifier byte — Shift, Ctrl, Alt… (hex)
- **CHAR**: decoded ASCII character

Scrolling log of the last 7 key presses.

Supported layouts: QWERTY (US) · AZERTY (FR) · QWERTZ (DE)

> **Escape key:** in keyboard diagnostic mode, Escape does **not** exit when a
> calibration is stored in flash (use the on-screen BACK button instead).
> If no calibration exists, Escape acts as a fallback exit.

### MOUSE — USB HID mouse

- Cursor X / Y position (delta-based, independent from touch calibration)
- Left / right button state (PRESSED / released)
- Graphical cursor visible across the entire screen

Exit: left-click the BACK button at the bottom of the screen.

### JOYSTICK — Sega Mega Drive gamepad

Real-time state of all 8 buttons: UP / DOWN / LEFT / RIGHT / A / B / C / START.

Navigation mapping from other menus:
- **C** = Confirm
- **A** = Back
- D-pad = directional navigation (with auto-repeat)

> **A button in diagnostic mode:** like the keyboard, A does **not** exit when a valid
> calibration is stored. Use the on-screen BACK button.

---

## Graphical assets — icon format

Menu tile icons are installed from `res/pic/` (SD card) to external flash (W25Q64)
on first boot.

| Parameter | Value |
|---|---|
| Size | **exactly 80 × 80 pixels** |
| Source format | **24-bit BMP, no compression** |
| Flash storage | Converted to RGB565 (2 bytes/pixel — 12 800 bytes/icon) |
| PNG | **Not supported** |

An incorrectly-sized image will appear cropped or corrupted.

To add icons: place files in `res/pic/` at the project root.
The build script copies them automatically to `out/.../release/pic/`.
