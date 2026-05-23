#include <Arduino.h>
#include <lvgl.h>

#include "BLE_HID.h"
#include "MacroPad.h"
#include "Rotary_Encoder.h"
#include "Config.h"
#include "Display.h"
#include "TaskSync.h"
#include "ui.h"

// ----- BLE + input devices -----
BLE_HID ble;

// Key 0=pause/continue, 1=skip, 2=next. Keys 3-5 passed through as HID.
static uint8_t keymap[ROWS * COLS] = { 0x00, 0x00, 0x00, 'd', 'e', 0xF0 };  // 0xF0 = KEY_PLAY_PAUSE

MacroPad keypad(ble, keymap, ROWS, COLS, ROW_PINS, COL_PINS);
Encoder  encoder(ble, PIN_ENCODER_A, PIN_ENCODER_B);

// ----- Pomodoro state -----
static size_t        currentPhase       = 0;
static int           remainingSeconds   = PHASES[0].minutes * 60;
static bool          timerRunning       = false;
static bool          waitingForContinue = false;  // true when phase ended, awaiting key press
static unsigned long lastSecondTick     = 0;

// ----- Tasks -----
static char   tasks[MAX_TASKS][TASK_MAX_LEN] = {
    "grasp map", "test", "orca handoff", "joint map*", "teleop"
};
static size_t taskCount    = 5;
static size_t taskOffset   = 0;  // index of the top-visible task in the 5-row queue
static int    taskSelected = 0;  // 0–4: which visible row is highlighted

static bool timerDirty = true;
static bool tasksDirty = true;

// WORK phases = black screen + task list; REST/LONG REST = white screen, no tasks.
static bool isWorkPhase(size_t idx) {
    return strcmp(PHASES[idx].label, "work") == 0;
}

// ----- Task parsing -----
static void applyTasks(const char* payload) {
    taskCount = 0;
    const char* p = payload;
    while (*p && taskCount < MAX_TASKS) {
        const char* nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        while (len > 0 && (p[len-1] == '\r' || p[len-1] == ' ')) len--;
        if (len > 0) {
            size_t n = len < TASK_MAX_LEN - 1 ? len : TASK_MAX_LEN - 1;
            memcpy(tasks[taskCount], p, n);
            tasks[taskCount][n] = '\0';
            // Lowercase-convert: monogram_lower_32 has no uppercase glyphs
            for (char* q = tasks[taskCount]; *q; q++)
                if (*q >= 'A' && *q <= 'Z') *q += 32;
            taskCount++;
        }
        p = nl ? nl + 1 : p + strlen(p);
    }
    taskOffset   = 0;
    taskSelected = 0;
    tasksDirty   = true;
}

// ----- Pomodoro control -----
static void advancePhase() {
    currentPhase = (currentPhase + 1) % PHASE_COUNT;
    remainingSeconds = PHASES[currentPhase].minutes * 60;
    timerDirty = true;
    tasksDirty = true;  // work↔rest switch changes task list visibility
}

static void skipPhase() {
    waitingForContinue = false;
    advancePhase();
    lastSecondTick = 0;  // give the new phase its full first second
}

static void togglePause() { timerRunning = !timerRunning; timerDirty = true; }

static void handleTimer() {
    if (!timerRunning) return;
    unsigned long now = millis();
    if (lastSecondTick == 0) { lastSecondTick = now; return; }
    if (now - lastSecondTick >= 1000) {
        lastSecondTick += 1000;
        if (remainingSeconds > 0) { remainingSeconds--; timerDirty = true; }
        if (remainingSeconds == 0) {
            timerRunning = false;
            waitingForContinue = true;
            // advancePhase() is deferred until the user presses the pause key
        }
    }
}

// ----- Input handling -----
static bool handleKeyEvent(uint8_t index, uint8_t, bool pressed) {
    if (index == PAUSE_KEY_INDEX) {
        if (pressed) {
            if (waitingForContinue) {
                bool wasRest = !isWorkPhase(currentPhase);
                waitingForContinue = false;
                advancePhase();
                // After a rest phase, advance the task queue so the next task is on top
                if (wasRest && taskCount > 1 && taskOffset < taskCount - 1) {
                    taskOffset++;
                    taskSelected = 0;
                }
                lastSecondTick = 0;
                timerRunning   = true;
            } else {
                togglePause();
            }
        }
        return true;
    }

    if (index == SKIP_KEY_INDEX) {
        if (pressed) {
            bool wasWaiting = waitingForContinue;
            skipPhase();
            if (wasWaiting) timerRunning = true;  // auto-start when skipping a finished phase
        }
        return true;
    }

    if (index == NEXT_KEY_INDEX) {
        if (pressed && taskCount > 0 && taskOffset < taskCount - 1) {
            taskOffset++;
            taskSelected = 0;
            tasksDirty   = true;
        }
        return true;
    }

    return false;
}

static void handleEncoder() {
    encoder.handle();  // sends BLE HID volume up/down; button handled by keymap
}

// ----- Arduino entry points -----
void setup() {
    Serial.begin(115200);
    ble.begin("ESP32 MacroPad");
    setupTaskService(ble);
    keypad.begin();
    keypad.setKeyHandler(handleKeyEvent);
    encoder.begin();
    setupDisplay();
    createUI();
    updateTimer(remainingSeconds, isWorkPhase(currentPhase), waitingForContinue);
    updateTaskQueue(tasks, taskCount, taskOffset, (size_t)taskSelected);
}

void loop() {
    keypad.handleKeypad();
    handleEncoder();
    handleTimer();

    static char payload[MAX_TASKS * TASK_MAX_LEN + 32];
    if (getNextTaskPayload(payload, sizeof(payload))) applyTasks(payload);

    if (timerDirty) {
        updateTimer(remainingSeconds, isWorkPhase(currentPhase), waitingForContinue);
        timerDirty = false;
    }
    if (tasksDirty) {
        updateTaskQueue(tasks, taskCount, taskOffset, (size_t)taskSelected);
        tasksDirty = false;
    }

    static unsigned long lastTick = 0;
    unsigned long now = millis();
    if (lastTick == 0) lastTick = now;
    lv_tick_inc(now - lastTick);
    lastTick = now;
    lv_timer_handler();
    delay(5);
}
