#ifndef MACROPAD_CONFIG_H
#define MACROPAD_CONFIG_H

#include <Arduino.h>
#include <string.h>

// Keypad matrix (2R × 3C)
static constexpr uint8_t ROWS = 2;
static constexpr uint8_t COLS = 3;
// Defined once in Config.cpp — avoids duplicate copies in every translation unit.
extern const uint8_t ROW_PINS[ROWS];  // {1, 5}
extern const uint8_t COL_PINS[COLS];  // {4, 21, 8}

// Encoder
static constexpr uint8_t PIN_ENCODER_A = 3;
static constexpr uint8_t PIN_ENCODER_B = 10;

#endif
