#include "Screen.h"

static constexpr uint16_t COLOR_FG = 0xFFFF;  // white
static constexpr uint16_t COLOR_BG = 0x0000;  // black
static constexpr uint32_t FADE_MS  = 400;

void Screen::begin() {
    _bus = new Arduino_ESP32SPI(PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_SCK, PIN_TFT_MOSI, PIN_TFT_MISO);
    // The constructor wants the panel's native portrait size; rotation 1 turns it
    // into the 280x240 landscape surface the host renders for.
    _gfx = new Arduino_ST7789(_bus, PIN_TFT_RST, TFT_ROTATION, true,
                              TFT_HEIGHT, TFT_WIDTH, TFT_OFFSET_X, TFT_OFFSET_Y);
    _gfx->begin();
    _gfx->fillScreen(COLOR_BG);
    _gfx->setTextColor(COLOR_FG, COLOR_BG);
    // Safety net only — the host wraps to the exact column count before sending.
    _gfx->setTextWrap(true);

    if (PIN_TFT_BL >= 0) pinMode(PIN_TFT_BL, OUTPUT);
    setBrightness(BRIGHTNESS);
    _lastActivityMs = millis();
}

void Screen::drawFrame(JsonArrayConst lines) {
    uint8_t incoming = lines.size();
    if (incoming > MAX_LINES) incoming = MAX_LINES;

    // A changed size anywhere shifts every line below it, so the cheap per-line
    // diff is only valid while the size vector is identical.
    bool layoutChanged = (incoming != _count);
    for (uint8_t i = 0; !layoutChanged && i < incoming; i++) {
        if (lines[i][0].as<uint8_t>() != _size[i]) layoutChanged = true;
    }

    int y = 0;
    for (uint8_t i = 0; i < incoming; i++) {
        uint8_t size = lines[i][0].as<uint8_t>();
        if (size < 1) size = 1;
        if (size > 8) size = 8;
        const char* text = lines[i][1].as<const char*>();
        if (!text) text = "";

        if (layoutChanged || strncmp(text, _text[i], MAX_CHARS) != 0) {
            drawLine(i, y, size, text);
        }
        y += CELL_H * size;
    }

    if (y < TFT_HEIGHT && (layoutChanged || incoming < _count)) clearFrom(y, incoming);
    _count = incoming;
}

void Screen::drawLine(uint8_t index, int y, uint8_t size, const char* text) {
    _gfx->fillRect(0, y, TFT_WIDTH, CELL_H * size, COLOR_BG);
    _gfx->setTextSize(size);
    _gfx->setCursor(0, y);
    _gfx->print(text);

    strncpy(_text[index], text, MAX_CHARS);
    _text[index][MAX_CHARS] = '\0';  // strncpy does not terminate on truncation
    _size[index] = size;
}

// Blank everything below y and forget the cached lines from `fromIndex` on, so a
// line that reappears there is redrawn. Lines above are still on screen and stay
// cached — invalidating those too would force a full repaint on every frame that
// merely got shorter, e.g. ticking a task off the pomodoro list.
void Screen::clearFrom(int y, uint8_t fromIndex) {
    _gfx->fillRect(0, y, TFT_WIDTH, TFT_HEIGHT - y, COLOR_BG);
    for (uint8_t i = fromIndex; i < MAX_LINES; i++) _text[i][0] = '\0';
}

void Screen::showStatus(const char* title, const char* detail) {
    _gfx->fillScreen(COLOR_BG);
    _gfx->setTextSize(2);
    _gfx->setCursor(0, 0);
    _gfx->print(title);
    _gfx->setTextSize(1);
    _gfx->setCursor(0, CELL_H * 2 + 8);
    _gfx->print(detail);
    _count = 0;
    for (uint8_t i = 0; i < MAX_LINES; i++) _text[i][0] = '\0';
}

// ── Backlight ────────────────────────────────────────────────────────────────

void Screen::setBrightness(uint8_t value) {
    _current = value;
    if (PIN_TFT_BL >= 0) analogWrite(PIN_TFT_BL, value);
}

void Screen::startFade(uint8_t from, uint8_t to, uint32_t durationMs) {
    _fadeFrom  = from;
    _fadeTo    = to;
    _fadeStart = millis();
    _fadeMs    = durationMs;
    _fading    = true;
    setBrightness(from);
}

void Screen::tickFade() {
    if (!_fading) return;
    uint32_t elapsed = millis() - _fadeStart;
    if (elapsed >= _fadeMs) {
        setBrightness(_fadeTo);
        _fading = false;
        return;
    }
    // Linear interpolation in int32 space: the product can exceed 16 bits.
    int32_t span = (int32_t)_fadeTo - (int32_t)_fadeFrom;
    setBrightness((uint8_t)((int32_t)_fadeFrom + span * (int32_t)elapsed / (int32_t)_fadeMs));
}

void Screen::noteActivity() {
    _lastActivityMs = millis();
    if (_target != BRIGHTNESS) {
        _target = BRIGHTNESS;
        startFade(_current, BRIGHTNESS, FADE_MS);  // fade up from wherever we are
    }
}

void Screen::tick() {
    tickFade();
    uint32_t idle = millis() - _lastActivityMs;
    uint8_t wanted = BRIGHTNESS;
    if (idle >= SLEEP_TIMEOUT_MS)   wanted = 0;
    else if (idle >= DIM_TIMEOUT_MS) wanted = DIM_BRIGHTNESS;

    if (wanted != _target) {
        _target = wanted;
        startFade(_current, wanted, FADE_MS);
    }
}
