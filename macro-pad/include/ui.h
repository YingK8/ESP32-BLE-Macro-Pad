#pragma once
#include <Arduino.h>
#include "Config.h"

// Creates the single LVGL label that renders the entire screen as a character grid.
// Must be called once after lv_init() and setupDisplay().
void createUI(void);

// Stores timer state and redraws the screen.
// phase    — PHASES[currentPhase].label: "work", "rest", or "long rest"
// running  — true while the countdown is active
// waiting  — true when the phase has expired and is waiting for a keypress to advance
// sessions — number of completed work sessions (shown as colored dots on break screens)
void updateTimer(int remainingSecs, const char* phase, bool running,
                 bool waitingContinue, int completedSessions);

// Stores task list state and redraws the screen.
// offset   — index of the first visible task in the queue
// selected — which visible row (0-based) has the < cursor
void updateTaskQueue(const char tasks[][TASK_MAX_LEN], size_t count,
                     size_t offset, size_t selected);

// Advance one step in a 4-position pixel-shift cycle (±2 px).
// Call every SHIFT_INTERVAL_MS to prevent image retention.
void shiftPixels();

// Switch between full UI (sleeping=false) and sleep screen (sleeping=true).
// Sleep screen shows only the timer + phase label, centered, for burn-in safety.
void setSleepMode(bool sleeping);
