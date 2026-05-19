#ifndef MACROPAD_CONFIG_H
#define MACROPAD_CONFIG_H

#include <Arduino.h>

// Keypad matrix
static constexpr uint8_t ROWS = 2;
static constexpr uint8_t COLS = 3;
static constexpr uint8_t ROW_PINS[ROWS] = {5, 1};
static constexpr uint8_t COL_PINS[COLS] = {4, 20, 8};
static constexpr uint8_t PAUSE_KEY_INDEX = 0; // top-left key (row 0, col 0)
static constexpr uint8_t ENCODER_CLICK_INDEX = 2; // top-right key (row 0, col 2)

// Encoder
static constexpr uint8_t PIN_ENCODER_A = 9;
static constexpr uint8_t PIN_ENCODER_B = 21;
static constexpr int PIN_ENCODER_BUTTON = -1; // click is wired into the matrix

// Display (ST7789 240x280, SPI)
static constexpr int PIN_TFT_SCK = -1;   // TODO: set SCK
static constexpr int PIN_TFT_MOSI = 10;  // MOSI/SDA
static constexpr int PIN_TFT_MISO = -1;  // unused
static constexpr int PIN_TFT_DC = -1;    // TODO: set DC
static constexpr int PIN_TFT_CS = -1;    // TODO: set CS
static constexpr int PIN_TFT_RST = -1;   // TODO: set RST
static constexpr int PIN_TFT_BL = -1;    // TODO: set backlight (optional)

static constexpr int TFT_WIDTH = 240;
static constexpr int TFT_HEIGHT = 280;
static constexpr int TFT_ROTATION = 0;
static constexpr int TFT_OFFSET_X = 0;
static constexpr int TFT_OFFSET_Y = 0;

// Tasks
static constexpr size_t MAX_TASKS = 12;

#endif
