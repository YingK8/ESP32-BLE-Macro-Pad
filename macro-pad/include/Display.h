#pragma once

// Initialises the ST7789 panel and LVGL (display driver, draw buffers, tick).
// Call once before any lv_* or UI functions.
void setupDisplay();
