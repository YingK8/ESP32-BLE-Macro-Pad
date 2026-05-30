#pragma once
#include <Arduino.h>
#include "Config.h"

// Build the full LVGL screen — call once after lv_init().
void createUI(void);

// Update the timer display and screen color scheme.
// isWork=true → black bg, white text, task list visible.
// isWork=false → white bg, black text, task list hidden.
// waitingContinue=true → timer is paused at 00:00 waiting for pause key.
void updateTimer(int remainingSecs, bool isWork, bool waitingContinue);

// Rebuild the 5-row task queue starting at tasks[offset].
// selected (0–4): which visible row is highlighted (white bg, black text).
// Rows beyond taskCount are hidden. Has no effect in rest mode.
void updateTaskQueue(const char tasks[][TASK_MAX_LEN], size_t count,
                     size_t offset, size_t selected);
