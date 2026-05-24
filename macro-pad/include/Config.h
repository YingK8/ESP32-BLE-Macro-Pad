#ifndef MACROPAD_CONFIG_H
#define MACROPAD_CONFIG_H

#include <Arduino.h>
#include <string.h>

// Keypad matrix (2R × 3C)
static constexpr uint8_t ROWS = 2;
static constexpr uint8_t COLS = 3;
// Defined once in Config.cpp — avoids duplicate copies in every translation unit.
extern const uint8_t ROW_PINS[ROWS];  // {1, 5}
extern const uint8_t COL_PINS[COLS];  // {4, 20, 8}

// Encoder
static constexpr uint8_t PIN_ENCODER_A = 3;
static constexpr uint8_t PIN_ENCODER_B = 10;

// Display — ST7789 240×280, 4-wire SPI
static constexpr int PIN_TFT_SCK  = 7;
static constexpr int PIN_TFT_MOSI = 6;
static constexpr int PIN_TFT_MISO = -1;
static constexpr int PIN_TFT_DC   = 2;
static constexpr int PIN_TFT_CS   = 0;
static constexpr int PIN_TFT_RST  = 9;
static constexpr int PIN_TFT_BL   = 21;

// Logical (post-rotation) dimensions; physical panel is portrait 240×280.
static constexpr int TFT_WIDTH    = 280;
static constexpr int TFT_HEIGHT   = 240;
static constexpr int TFT_ROTATION = 1;
static constexpr int TFT_OFFSET_X = 0;
static constexpr int TFT_OFFSET_Y = 20;

// UI grid padding
static constexpr int UI_PADDING     = 20;
static constexpr int UI_PADDING_TOP = 30;

// Pomodoro
struct Phase { const char* label; uint16_t minutes; bool bigDot; };

// PHASE_COUNT must match the array in Config.cpp.
static constexpr size_t PHASE_COUNT      = 8;
static constexpr size_t LONG_BREAK_PHASE = PHASE_COUNT - 1;
extern const Phase PHASES[PHASE_COUNT];

// Tasks
static constexpr size_t MAX_TASKS    = 12;
static constexpr size_t TASK_MAX_LEN = 32;  // max chars per stored task name (incl. null)

#endif
