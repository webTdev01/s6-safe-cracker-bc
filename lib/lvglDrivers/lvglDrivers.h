#ifndef LVGL_DRIVERS_H
#define LVGL_DRIVERS_H

#include "lvgl.h"

#include <Arduino.h>
#include "STM32FreeRTOS.h"

void mySetup();
void myTask(void *pvParameters);

bool lvglLock(TickType_t xBlockTime = portMAX_DELAY);
bool lvglUnlock();

#endif // LVGL_DRIVERS_H