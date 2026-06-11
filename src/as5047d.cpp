#include "as5047d.h"
#include <Arduino.h>

#define AS5047D_PIN_CS      9
#define AS5047D_PIN_MOSI    7
#define AS5047D_PIN_MISO    8
#define AS5047D_PIN_SCK     6
#define AS5047D_REG_NOP     0x0000
#define AS5047D_REG_ANGLEUNC 0x3FFE

static uint16_t softwareSPI_transfer16(uint16_t value) {
    uint16_t out = 0;
    for (int i = 15; i >= 0; i--) {
        digitalWrite(AS5047D_PIN_MOSI, (value & (1 << i)) ? HIGH : LOW);
        delayMicroseconds(5);
        digitalWrite(AS5047D_PIN_SCK, HIGH);
        delayMicroseconds(5);
        digitalWrite(AS5047D_PIN_SCK, LOW);
        delayMicroseconds(5);
        if (digitalRead(AS5047D_PIN_MISO)) out |= (1 << i);
    }
    return out;
}

uint16_t AS5047D_ReadRaw() {
    uint16_t command = 0x4000 | AS5047D_REG_ANGLEUNC;
    digitalWrite(AS5047D_PIN_CS, LOW);
    delayMicroseconds(5);
    softwareSPI_transfer16(command);
    digitalWrite(AS5047D_PIN_CS, HIGH);
    delayMicroseconds(10);
    digitalWrite(AS5047D_PIN_CS, LOW);
    delayMicroseconds(5);
    uint16_t response = softwareSPI_transfer16(AS5047D_REG_NOP);
    digitalWrite(AS5047D_PIN_CS, HIGH);
    return (response & 0x3FFF);
}

void AS5047D_Init(void) {
    pinMode(AS5047D_PIN_CS,   OUTPUT);
    pinMode(AS5047D_PIN_SCK,  OUTPUT);
    pinMode(AS5047D_PIN_MOSI, OUTPUT);
    pinMode(AS5047D_PIN_MISO, INPUT_PULLUP);
    digitalWrite(AS5047D_PIN_CS,  HIGH);
    digitalWrite(AS5047D_PIN_SCK, LOW);
    delay(10);
}

float AS5047D_RawToDeg(uint16_t raw) {
    return raw * 360.0f / 16384.0f;
}
