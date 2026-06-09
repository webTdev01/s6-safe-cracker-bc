#pragma once
#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum { MENU, SAFE_CRACKER, SPIN_SURVIVE } ActiveGame_t;

typedef enum {
    SS_IDLE,       // waiting for player to spin the magnet
    SS_SPINNING,   // needle decelerating after button press
    SS_RESULT,     // showing result overlay (2 s pause)
    SS_DEAD,       // death screen — waiting for restart tap
    SS_VICTORY     // survived all 4 rounds — waiting for menu tap
} SpinSurviveState_t;

void SS_CreateScreen();
void SS_ShowScreen();
void SS_Update(float angleDeg, float speedDegPerSec);
extern bool ss_button_pressed;
extern bool ss_screen_created;
