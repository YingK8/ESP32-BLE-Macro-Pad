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

// Display — ST7789 240×280 panel, 4-wire SPI
static constexpr int PIN_TFT_SCK  = 7;
static constexpr int PIN_TFT_MOSI = 6;
static constexpr int PIN_TFT_MISO = -1;
static constexpr int PIN_TFT_DC   = 2;
static constexpr int PIN_TFT_CS   = 0;
static constexpr int PIN_TFT_RST  = 9;
static constexpr int PIN_TFT_BL   = 20;  // PWM backlight; set < 0 if wired straight to 3V3

static constexpr uint8_t  BRIGHTNESS       = 150;    // 0-255 PWM when active
static constexpr uint8_t  DIM_BRIGHTNESS   = 50;     // PWM when idle — readable but quiet
static constexpr uint32_t DIM_TIMEOUT_MS   = 20000;  // idle before dimming
static constexpr uint32_t SLEEP_TIMEOUT_MS = 300000; // idle before the backlight goes off

// Logical (post-rotation) dimensions; the physical panel is portrait 240×280.
// These must match PANEL_W/PANEL_H in host/src/macropad_host/frame.py.
static constexpr int TFT_WIDTH    = 280;
static constexpr int TFT_HEIGHT   = 240;
static constexpr int TFT_ROTATION = 1;
static constexpr int TFT_OFFSET_X = 0;
static constexpr int TFT_OFFSET_Y = 20;  // the usual row offset on 240×280 ST7789 panels

#endif
