#pragma once
#include <Arduino.h>

// Initialises the ST7789 panel and LVGL (display driver, draw buffers, tick).
// Call once before any lv_* or UI functions.
void setupDisplay();

// Set backlight PWM level (0=off, 255=full). No-op if PIN_TFT_BL < 0.
void setBrightness(uint8_t val);

// Begin a linear brightness fade from `from` to `to` over `durationMs` milliseconds.
void startFade(uint8_t from, uint8_t to, uint32_t durationMs);

// Advance the in-progress fade one step; call every loop iteration.
// Returns true while a fade is still running.
bool tickFade();

// True if a fade is currently in progress.
bool isFading();

// Abort any in-progress fade and snap brightness to `val`.
void cancelFade(uint8_t val);
