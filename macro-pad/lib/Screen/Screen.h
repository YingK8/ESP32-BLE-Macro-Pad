#ifndef SCREEN_H
#define SCREEN_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>

#include "Config.h"

// Built-in GFX font: a 5x7 glyph in a 6x8 px cell, scaled by setTextSize().
static constexpr int CELL_W = 6;
static constexpr int CELL_H = 8;

// Worst case is a screen full of size-1 lines. Both derived from the panel size,
// so they stay correct if the panel ever changes.
static constexpr uint8_t MAX_LINES = TFT_HEIGHT / CELL_H;   // 30
static constexpr uint8_t MAX_CHARS = TFT_WIDTH / CELL_W;    // 46

// Renders host-supplied text lines and owns the backlight state machine.
//
// No framebuffer: a 280x240x16bpp canvas is 134 KB against the C3's 320 KB of
// RAM. Instead we cache the last frame as text and repaint only the lines whose
// content changed, which is both cheaper and flicker-free.
class Screen {
public:
    void begin();

    // Draw a frame. `lines` is the wire format's "l" array: [[size, "text"], ...].
    void drawFrame(JsonArrayConst lines);

    // Built-in fallback screen, used before the host connects. Never diffed.
    void showStatus(const char* title, const char* detail);

    // Call every loop(): advances any backlight fade and applies dim/sleep timeouts.
    void tick();

    // Reset the idle timers and wake the backlight. Call on any user input.
    void noteActivity();

private:
    void drawLine(uint8_t index, int y, uint8_t size, const char* text);
    void clearFrom(int y, uint8_t fromIndex);
    void setBrightness(uint8_t value);
    void startFade(uint8_t from, uint8_t to, uint32_t durationMs);
    void tickFade();

    Arduino_DataBus* _bus = nullptr;
    Arduino_GFX*     _gfx = nullptr;

    // Last-drawn frame, kept as fixed buffers rather than String to avoid heap
    // churn on a device that redraws once a second for hours.
    char    _text[MAX_LINES][MAX_CHARS + 1] = {};
    uint8_t _size[MAX_LINES] = {};
    uint8_t _count = 0;

    uint32_t _lastActivityMs = 0;
    uint8_t  _current    = BRIGHTNESS;  // current PWM level
    uint8_t  _target     = BRIGHTNESS;  // level the idle state machine wants
    uint8_t  _fadeFrom   = BRIGHTNESS;
    uint8_t  _fadeTo     = BRIGHTNESS;
    uint32_t _fadeStart  = 0;
    uint32_t _fadeMs     = 0;
    bool     _fading     = false;
};

#endif  // SCREEN_H
