#ifndef MACROPAD_CONFIG_H
#define MACROPAD_CONFIG_H

#include <Arduino.h>

// Keypad matrix
static constexpr uint8_t ROWS = 2;
static constexpr uint8_t COLS = 3;
static constexpr uint8_t ROW_PINS[ROWS] = {1, 5};             // Row 0 -> 1, Row 1 -> 5
static constexpr uint8_t COL_PINS[COLS] = {4, 20, 8};          // Col 0 -> 4, Col 1 -> 20 (RX), Col 2 -> 8
static constexpr uint8_t PAUSE_KEY_INDEX = 0;                 // top-left key (row 0, col 0)
static constexpr uint8_t ENCODER_CLICK_INDEX = 2;             // top-right key (row 0, col 2)

// Encoder
static constexpr uint8_t PIN_ENCODER_A = 3;                   // Moved from 9 (Safe from boot restrictions)
static constexpr uint8_t PIN_ENCODER_B = 10;                  // Moved from 21 (Safe from boot restrictions)
static constexpr int PIN_ENCODER_BUTTON = -1;                 // click is wired into the matrix

// Display (ST7789 240x280, SPI)
static constexpr int PIN_TFT_SCK = 7;                          // SCL Pin
static constexpr int PIN_TFT_MOSI = 6;                         // SDA Pin
static constexpr int PIN_TFT_MISO = -1;                        // Unused (Screen is write-only)
static constexpr int PIN_TFT_DC = 2;                           // D/C Pin
static constexpr int PIN_TFT_CS = 0;                           // CS Pin
static constexpr int PIN_TFT_RST = 9;                          // RESET Pin (Safe strapping location)
static constexpr int PIN_TFT_BL = 21;                          // Backlight (TX Pin)

static constexpr int TFT_WIDTH = 240;
static constexpr int TFT_HEIGHT = 280;
static constexpr int TFT_ROTATION = 0;
static constexpr int TFT_OFFSET_X = 0;
static constexpr int TFT_OFFSET_Y = 0;

// Tasks
static constexpr size_t MAX_TASKS = 12;

#endif