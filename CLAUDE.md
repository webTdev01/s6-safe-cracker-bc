# CLAUDE.md — Contraintes projet s6-safe-cracker-bc

## Cible & build
- Carte : STM32F746NG-DISCO (480×272 LCD, FT5336 touch)
- Commande de build : `pio run -e disco_f746ng`  (ne jamais flasher — l'étudiant flashe manuellement)
- Framework : PlatformIO + Arduino + FreeRTOS (STM32FreeRTOS-10.3.2) + LVGL v9.2.2

## Capteur angulaire
- Composant physique : **AS5047D** (14 bits, 16384 pos/tour, SPI mode 1)
- Protocole : SPI bit-bang (CLK sur D6/SPI5, MOSI/MISO sur D7-D8/SPI2 → pas de SPI matériel unique)
- Séquence 2 trames : trame commande, puis trame NOP pour récupérer la donnée
- Brochage : CS=D9 (pin 9), CLK=D6 (pin 6), MOSI=D7 (pin 7), MISO=D8 (pin 8)
- Note historique : l'ancien symbole `AS5047P_` a été renommé `AS5047D_` ; le composant est bien un AS5047**D**

## Contraintes strictes (ne jamais enfreindre)

1. **Bibliothèques intouchables** : ne jamais modifier `lib/lvglDrivers/`, `lib/lvgl/`,
   `lib/STM32746G-Discovery/`, `lib/STM32FreeRTOS-10.3.2/`, `lib/Components/`, `lib/Utilities/`,
   ni les flags de `platformio.ini`, ni l'architecture FreeRTOS 2 tâches (`mySetup`/`myTask`/`lvglTask`).

2. **Thread safety LVGL** : tout appel LVGL depuis `myTask()` doit être encadré de
   `lvglLock(portMAX_DELAY)` / `lvglUnlock()`. Les callbacks LVGL (event/timer/anim) tournent
   dans `lvglTask` où le mutex est déjà tenu — **jamais de lock à l'intérieur d'un callback** (deadlock).

3. **lv_obj_get_child sur arc interdit** : ne jamais appeler `lv_obj_get_child()` sur un objet
   `lv_arc` — l'arc n'a pas d'enfants ici et cela corrompt le heap LVGL.
   (Exception valide : `lv_obj_get_child(btn_restart, 0)` dans spinSurvive.cpp — c'est un bouton.)

4. **API LVGL v9 uniquement** : ne pas utiliser `lv_anim_timeline_set_completed_cb`,
   `lv_anim_get_var`, ni aucune API v8. Vérifier dans `lib/lvgl` avant d'introduire une fonction.

5. **Police unique** : seule `lv_font_montserrat_14` est activée sur la cible.
   Ne pas référencer d'autre police.

6. **Pas de rotation transform sur les arcs** : `lv_obj_set_style_transform_rotation` sur un arc
   provoque un gel de l'affichage sur ce matériel.

7. **Timing SPI identique** : conserver les `delayMicroseconds(5)`, la séquence 2 trames,
   et le toggling CS exactement tels qu'ils sont écrits.

8. **Init paresseuse obligatoire** : ne jamais appeler un `Create*Screen()` avant que LVGL
   soit initialisé ; ne jamais créer d'objets LVGL dans `mySetup()` avant le moment actuel.

9. **Détection absence capteur** : la logique doit comparer la valeur brute `raw == 0x3FFF`,
   jamais un angle arrondi `angle == 359`.

## Fichiers sources
- `src/main.cpp` — jeu Safe Cracker (monolithique) + architecture FreeRTOS
- `src/as5047d.h` / `src/as5047d.cpp` — driver SPI bit-bang AS5047D
- `src/spinSurvive.h` / `src/spinSurvive.cpp` — jeu Spin & Survive (modulaire)
