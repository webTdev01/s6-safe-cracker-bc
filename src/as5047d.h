#pragma once
#include <stdint.h>

void     AS5047D_Init(void);
uint16_t AS5047D_ReadRaw(void);
float    AS5047D_RawToDeg(uint16_t raw);
