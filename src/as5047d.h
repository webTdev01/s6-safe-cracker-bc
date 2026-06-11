// Driver AS5047D — capteur angulaire magnétique 14 bits, interface SPI bit-bang
// Carte : STM32F746NG-DISCO  |  Auteur : [étudiant]  |  Date : 2026
// Brochage : CS=D9, CLK=D6, MOSI=D7, MISO=D8
#pragma once
#include <stdint.h>

// Initialise les GPIO du bus SPI logiciel (CS, SCK, MOSI en sortie, MISO en entrée pull-up)
void     AS5047D_Init(void);

// Lit l'angle brut sur 14 bits (0–16383). Retourne 0x3FFF si le capteur est absent.
uint16_t AS5047D_ReadRaw(void);

// Convertit une valeur brute 14 bits en degrés (0.0–359.978°)
float    AS5047D_RawToDeg(uint16_t raw);
