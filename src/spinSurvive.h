// Module Spin & Survive — mini-jeu de roulette russe sur STM32F746NG-DISCO
// Carte : STM32F746NG-DISCO  |  Auteur : [étudiant]  |  Date : 2026
// Appelé depuis main.cpp ; tous les appels LVGL de ce module se font
// depuis myTask() via lvglLock/lvglUnlock — ne jamais ajouter de lock dans un callback.
#pragma once
#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

// États de la machine d'états du jeu Spin & Survive
typedef enum {
    SS_IDLE,      // en attente : le barillet suit l'angle du capteur en temps réel
    SS_SPINNING,  // réservé (non utilisé dans l'implémentation actuelle)
    SS_RESULT,    // pause 2 s après un survie : affichage du résultat avant round suivant
    SS_DEAD,      // mort : overlay rouge affiché, attente du bouton "Rejouer"
    SS_VICTORY    // victoire : tous les 4 rounds passés, attente du bouton "Menu"
} SpinSurviveState_t;

// API publique — appelée depuis main.cpp
void SS_CreateScreen();  // crée tous les widgets LVGL ; ne charge PAS l'écran
void SS_ShowScreen();    // réinitialise l'état du jeu, puis appelle lv_scr_load()
void SS_Update(float angleDeg, float speedDegPerSec);
     // appelée par myTask() toutes les 30 ms, déjà dans le contexte lvglLock

// Drapeau positionné par main.cpp quand le bouton utilisateur est pressé
extern bool ss_button_pressed;
