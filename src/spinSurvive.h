// Module Spin & Survive — roulette russe sur STM32F746NG-DISCO
// Carte : STM32F746NG-DISCO  |  Auteur : [étudiant]  |  Date : 2026
// Appelé depuis main.cpp ; tous les appels LVGL de ce module se font
// depuis myTask() via lvglLock/lvglUnlock — ne jamais ajouter de lock dans un callback.
#pragma once
#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

// États de la machine d'états du jeu Spin & Survive
typedef enum {
    SS_HOME,       // écran d'accueil S&S (règles + JOUER)
    SS_WAIT_SPIN,  // barillet = angle capteur ; armement si vitesse > 360°/s × 3 ticks consécutifs
    SS_SPINNING,   // armé ; arrêt si vitesse < 10°/s × 10 ticks consécutifs (300 ms)
    SS_SUSPENSE,   // arrêté ; 700 ms "CLIC..." avant le résultat
    SS_RESULT,     // chambre vide : overlay 2200 ms puis round suivant
    SS_DEAD,       // balle : toutes les balles révélées ; boutons REJOUER / MENU
    SS_VICTORY     // 4 rounds survécus ; boutons REJOUER / MENU
} SS_State_t;

// API publique — appelée depuis main.cpp
void SS_ShowHome(void);
     // init paresseuse des deux écrans, réinitialise l'état, charge scr_ss_home
void SS_Update(float angleDeg, float speedDegPerSec);
     // appelée par myTask() toutes les 30 ms, déjà dans le contexte lvglLock
void SS_SetSensorError(bool visible);
     // affiche/masque le label "CAPTEUR NON DETECTE" sur scr_ss_game
