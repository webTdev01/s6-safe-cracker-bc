// Driver AS5047D — SPI bit-bang pour capteur angulaire magnétique 14 bits
// Carte : STM32F746NG-DISCO  |  Auteur : [étudiant]  |  Date : 2026
//
// Pourquoi SPI bit-bang ?
//   CLK est sur D6 (SPI5) et MOSI/MISO sont sur D7/D8 (SPI2) : aucun périphérique
//   SPI matériel ne regroupe ces trois broches ; le bit-bang est la seule option.
//
// Format de trame AS5047D (16 bits, MSB en premier) :
//   bit15 : parité paire sur les bits 14:0 (calculée avant envoi)
//   bit14 : 0 = écriture, 1 = lecture
//   bits13:0 : adresse du registre (ou donnée en écriture)
//
// Séquence de lecture en deux trames :
//   1ère trame : envoi de la commande de lecture (ex. 0x7FFE pour ANGLEUNC)
//   2ème trame : envoi NOP (0x0000) pendant lequel le capteur renvoie la donnée
//   La réponse est masquée à 14 bits (bit15 = parité, bit14 = erreur/alarme ignorés).
//
// Mode SPI 1 (CPOL=0, CPHA=1) :
//   SCK repos = bas, données échantillonnées sur le front montant de SCK.
//   Les delayMicroseconds(5) garantissent tCSS ≥ 350 ns et tSCLK ≥ 750 ns.
#include "as5047d.h"
#include <Arduino.h>

// Broches SPI bit-bang
#define AS5047D_PIN_CS      9
#define AS5047D_PIN_MOSI    7
#define AS5047D_PIN_MISO    8
#define AS5047D_PIN_SCK     6

// Adresses de registres AS5047D utilisées
#define AS5047D_REG_NOP      0x0000  // trame vide (NOP), utilisée comme 2ème trame pour lire la réponse
#define AS5047D_REG_ANGLEUNC 0x3FFE  // registre ANGLEUNC : angle non compensé 14 bits

// Transfère 16 bits en SPI bit-bang, MSB en premier, mode 1 (SCK repos bas, échantillon sur front montant)
static uint16_t softwareSPI_transfer16(uint16_t value) {
    uint16_t out = 0;
    for (int i = 15; i >= 0; i--) {
        digitalWrite(AS5047D_PIN_MOSI, (value & (1 << i)) ? HIGH : LOW);
        delayMicroseconds(5);        // setup MOSI avant front montant
        digitalWrite(AS5047D_PIN_SCK, HIGH);
        delayMicroseconds(5);        // maintien SCK haut (tHIGH ≥ 350 ns)
        digitalWrite(AS5047D_PIN_SCK, LOW);
        delayMicroseconds(5);        // MISO stabilisé après front descendant
        if (digitalRead(AS5047D_PIN_MISO)) out |= (1 << i);
    }
    return out;
}

// Lit l'angle brut sur 14 bits via la séquence 2-trames du AS5047D.
// Commande : bit14=1 (lecture), adresse 0x3FFE → 0x4000|0x3FFE = 0x7FFE.
// bit15 de la commande = parité paire : 0x7FFE a 14 bits à 1 → parité = 0 → valeur finale 0x7FFE.
uint16_t AS5047D_ReadRaw() {
    uint16_t command = 0x4000 | AS5047D_REG_ANGLEUNC; // 0x7FFE (parité déjà correcte)
    // 1ère trame : envoi de la commande de lecture
    digitalWrite(AS5047D_PIN_CS, LOW);
    delayMicroseconds(5);
    softwareSPI_transfer16(command);
    digitalWrite(AS5047D_PIN_CS, HIGH);
    delayMicroseconds(10);  // tCSH minimum entre les deux trames
    // 2ème trame : NOP pour récupérer la donnée
    digitalWrite(AS5047D_PIN_CS, LOW);
    delayMicroseconds(5);
    uint16_t response = softwareSPI_transfer16(AS5047D_REG_NOP);
    digitalWrite(AS5047D_PIN_CS, HIGH);
    return (response & 0x3FFF); // masque : écarte parité (bit15) et bit d'erreur (bit14)
}

// Configure les GPIO du bus SPI bit-bang et attend la stabilisation du capteur.
void AS5047D_Init(void) {
    pinMode(AS5047D_PIN_CS,   OUTPUT);
    pinMode(AS5047D_PIN_SCK,  OUTPUT);
    pinMode(AS5047D_PIN_MOSI, OUTPUT);
    pinMode(AS5047D_PIN_MISO, INPUT_PULLUP);
    digitalWrite(AS5047D_PIN_CS,  HIGH); // CS inactif = haut
    digitalWrite(AS5047D_PIN_SCK, LOW);  // SCK repos = bas (mode 1)
    delay(10); // attente de démarrage du AS5047D (tVDD ≥ 10 ms après alimentation)
}

// Convertit une valeur brute 14 bits en angle en degrés : raw * 360 / 16384
float AS5047D_RawToDeg(uint16_t raw) {
    return raw * 360.0f / 16384.0f;
}
