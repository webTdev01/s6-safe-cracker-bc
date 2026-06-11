# Safe Cracker & Spin & Survive

Deux mini-jeux embarqués pilotés par un capteur angulaire magnétique AS5047D,
développés sur STM32F746NG-DISCO dans le cadre du projet d'interfacage du Semestre 6 (IUT de Cachan, 2025-2026).

---

## Matériel requis

| Composant | Description |
|---|---|
| STM32F746NG-DISCO | Carte Discovery, écran LCD 480×272, contrôleur tactile FT5336 |
| AS5047D | Capteur angulaire magnétique 14 bits (16 384 positions/tour), interface SPI |
| PCB custom | Adaptateur capteur vers connecteur J1 de la carte |

Table de câblage (connecteur J1 capteur → Arduino pins) :

| Signal | Arduino pin | Remarque |
|---|---|---|
| CLK | D6 | SPI5 SCK (bit-bang) |
| MOSI | D7 | SPI2 MOSI (bit-bang) |
| MISO | D8 | SPI2 MISO (bit-bang) |
| CS | D9 | Chip Select actif bas |
| VDD | 3V3 | Alimentation 3,3 V |
| GND | GND | |

---

## Stack logicielle

- **PlatformIO** — système de build, cible `disco_f746ng`
- **Arduino framework** — couche d'abstraction matérielle STM32
- **FreeRTOS (STM32FreeRTOS-10.3.2)** — deux tâches :
  - `lvglTask` : boucle de rendu et gestion des events LVGL
  - `myTask` : lecture capteur + mise à jour jeu toutes les 30 ms
- **LVGL v9.2.2** — interface graphique, police unique `lv_font_montserrat_14`
- **SPI bit-bang** (pas de SPI matériel) : CLK est sur SPI5 et MOSI/MISO sur SPI2 ;
  aucun périphérique matériel ne regroupe ces trois broches.

---

## Les deux jeux

### Safe Cracker
Le joueur doit trouver une combinaison de 3 angles secrets (générés aléatoirement,
espacés d'au moins 30° l'un de l'autre) en tournant le capteur.
- Tolérance de validation : ±10°
- Maintien requis : ~1,5 s (50 ticks × 30 ms) dans la zone verte
- Feedback : fond gris (loin) → orange (approche < 30°) → vert (zone < 10°)

### Spin & Survive
Roulette russe à 4 manches. Le joueur tourne le capteur librement ;
quand la vitesse passe sous 10°/s pendant 300 ms, le barillet s'arrête.
- 6 secteurs de 60° : les `N` premiers sont mortels à la manche `N`
- Manche 1 : 1 secteur mortel / 6 ; manche 4 : 4 secteurs mortels / 6
- Victoire : survivre aux 4 manches

---

## Compilation et flash

```
pio run -e disco_f746ng
```

Le flash est effectué manuellement via PlatformIO (bouton Upload) ou ST-Link.
Ne jamais flasher depuis un script automatique — risque d'interférence avec le débogage.

---

## Architecture du code

| Fichier | Rôle |
|---|---|
| `src/main.cpp` | Jeu Safe Cracker (monolithique), architecture FreeRTOS, `mySetup()` / `myTask()` |
| `src/as5047d.h` / `as5047d.cpp` | Driver SPI bit-bang AS5047D — `Init`, `ReadRaw`, `RawToDeg` |
| `src/spinSurvive.h` / `spinSurvive.cpp` | Jeu Spin & Survive — machine d'états, `SS_ShowScreen`, `SS_Update` |

Règle LVGL thread-safety : tout appel LVGL depuis `myTask()` doit être encadré de
`lvglLock(portMAX_DELAY)` / `lvglUnlock()`. Les callbacks LVGL (event/timer/anim)
tournent dans `lvglTask` et ne doivent jamais appeler `lvglLock` (risque de deadlock).

---

## Note sur le nommage

Les anciens symboles `AS5047P_CS`, `AS5047P_MOSI`, etc. ont été renommés en `AS5047D_PIN_CS`,
`AS5047D_PIN_MOSI`, etc. dans le refactoring de janvier 2026. Le composant physique est
bien un **AS5047D** (pas AS5047P). Voir `CLAUDE.md` pour la liste complète des contraintes projet.
