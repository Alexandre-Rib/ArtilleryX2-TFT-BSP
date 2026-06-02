/**
 * @file  mega9.h
 * @brief Driver MEGA9 — manette Sega Mega Drive 3 boutons via pont Arduino Nano
 *
 *  Protocole UART 115 200 baud, trames 3 octets :
 *    Octet 0 : 0xA5  (magic / sync)
 *    Octet 1 : 0x00 (manette absente) | 0x01 (manette présente)
 *    Octet 2 : bitmask boutons  (bits MEGA9_BTN_*)
 *
 *  Envoyé sur changement d'état + heartbeat toutes les 100 ms.
 *  Lien considéré mort si aucune trame reçue depuis MEGA9_LINK_TIMEOUT_MS.
 */

#ifndef _MEGA9_H_
#define _MEGA9_H_

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Bitmask boutons — Octet 2 du protocole
// ---------------------------------------------------------------------------
#define MEGA9_BTN_UP     (1u << 0)
#define MEGA9_BTN_DOWN   (1u << 1)
#define MEGA9_BTN_LEFT   (1u << 2)
#define MEGA9_BTN_RIGHT  (1u << 3)
#define MEGA9_BTN_C      (1u << 4)   // → NAVIGATION_BACK  (Escape)
#define MEGA9_BTN_B      (1u << 5)
#define MEGA9_BTN_A      (1u << 6)   // → NAVIGATION_CONFIRM (Enter)
#define MEGA9_BTN_START  (1u << 7)

// ---------------------------------------------------------------------------
// API publique
// ---------------------------------------------------------------------------

/**
 * @brief Initialise USART1 à 115 200 baud via le driver Serial/DMA.
 *        Appeler une seule fois, avant la boucle principale.
 */
void    Mega9_Init(void);

/**
 * @brief Draine le buffer DMA USART1 et avance la machine d'état de tramage.
 *        Appeler depuis la boucle principale (~1 ms) ET depuis BSP_YieldHook.
 */
void    Mega9_Process(void);

/** @return true si la manette est détectée sur le DB9 ET le lien Arduino est actif. */
bool    Mega9_IsConnected(void);

/** @return true si une trame valide a été reçue il y a moins de 500 ms. */
bool    Mega9_IsLinkAlive(void);

/**
 * @brief État courant des boutons (tenu maintenu).
 * @return bitmask MEGA9_BTN_* ; 0 si déconnecté ou lien mort.
 */
uint8_t Mega9_GetButtons(void);

/**
 * @brief Transitions 0→1 depuis le dernier appel (front montant, consommé à la lecture).
 *        Renvoie les bits des boutons qui viennent d'être pressés, puis les efface.
 */
uint8_t Mega9_GetNewButtons(void);

/** @brief Vide les fronts montants en attente (appeler lors des transitions de scène). */
void    Mega9_FlushEdges(void);

#endif /* _MEGA9_H_ */
