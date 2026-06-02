#include "Display.h"
#include "Config.h"
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

static Arduino_DataBus* gfxBus = nullptr;
static Arduino_GFX*     gfx    = nullptr;
static lv_disp_draw_buf_t drawBuf;
static lv_color_t buf1[TFT_WIDTH * 10];
static lv_color_t buf2[TFT_WIDTH * 10];

// LVGL calls this when a screen region is ready to blit to the panel.
static void lvglFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t*>(color_p), w, h);
    lv_disp_flush_ready(disp);
}

// Track current PWM level so cancelFade() knows where to fade from.
static uint8_t s_currBrightness = BRIGHTNESS;

void setBrightness(uint8_t val) {
    s_currBrightness = val;
    if (PIN_TFT_BL >= 0) analogWrite(PIN_TFT_BL, val);
}

// ── Fade state ────────────────────────────────────────────────────────────────
static uint8_t  s_fadeFrom  = 0;
static uint8_t  s_fadeTo    = 50;
static uint32_t s_fadeStart = 0;
static uint32_t s_fadeDur   = 0;
static bool     s_fading    = false;

void startFade(uint8_t from, uint8_t to, uint32_t durationMs) {
    s_fadeFrom  = from;
    s_fadeTo    = to;
    s_fadeStart = millis();
    s_fadeDur   = durationMs;
    s_fading    = true;
    setBrightness(from);
}

// Linear interpolation: val = from + (to-from) * t, where t = elapsed/duration.
bool tickFade() {
    if (!s_fading) return false;
    uint32_t elapsed = millis() - s_fadeStart;
    if (elapsed >= s_fadeDur) {
        setBrightness(s_fadeTo);
        s_fading = false;
        return false;
    }
    int val = (int)s_fadeFrom + ((int)s_fadeTo - (int)s_fadeFrom) * (int)elapsed / (int)s_fadeDur;
    setBrightness((uint8_t)val);
    return true;
}

bool isFading() { return s_fading; }

// Stop the current fade and begin a smooth fade-up to `to` from wherever
// the brightness currently is. Used to cancel a mid-fade dim.
void cancelFade(uint8_t to) {
    uint8_t from = s_currBrightness;  // mid-fade value captured from last setBrightness
    s_fading = false;
    startFade(from, to, 300);         // 300 ms fade-up from current level
}

void setupDisplay() {
    gfxBus = new Arduino_ESP32SPI(PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_SCK, PIN_TFT_MOSI, PIN_TFT_MISO);
    // Constructor takes native portrait dims (240×280); rotation=1 gives logical 280×240
    gfx = new Arduino_ST7789(gfxBus, PIN_TFT_RST, TFT_ROTATION, true, TFT_HEIGHT, TFT_WIDTH, TFT_OFFSET_X, TFT_OFFSET_Y);
    gfx->begin();
    gfx->fillScreen(0x0000);
    if (PIN_TFT_BL >= 0) {
        pinMode(PIN_TFT_BL, OUTPUT);
        analogWrite(PIN_TFT_BL, BRIGHTNESS);
    }

    lv_init();
    lv_disp_draw_buf_init(&drawBuf, buf1, buf2, TFT_WIDTH * 10);

    static lv_disp_drv_t dispDrv;
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res = TFT_WIDTH;
    dispDrv.ver_res = TFT_HEIGHT;
    dispDrv.flush_cb = lvglFlush;
    dispDrv.draw_buf = &drawBuf;
    lv_disp_drv_register(&dispDrv);
}
