#pragma once
#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SS_IDLE,       // waiting for player to spin the magnet
    SS_SPINNING,   // needle decelerating after button press
    SS_RESULT,     // showing result overlay (2 s pause)
    SS_DEAD,       // death screen — waiting for restart tap
    SS_VICTORY     // survived all 4 rounds — waiting for menu tap
} SpinSurviveState_t;

// Public API — called from main.cpp
void SS_CreateScreen();   // build all LVGL widgets, do NOT load screen
void SS_ShowScreen();     // reset game state, call lv_scr_load(scr_ss)
void SS_Update(float angleDeg, float speedDegPerSec);
      // called from myTask() every 30 ms, already inside lvglLock context

// Flag set by main.cpp when USER_BUTTON is pressed
extern bool ss_button_pressed;
