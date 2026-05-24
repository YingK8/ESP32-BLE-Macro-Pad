#include <Arduino.h>
#include <lvgl.h>

#include "BLE_HID.h"
#include "MacroPad.h"
#include "Rotary_Encoder.h"
#include "Config.h"
#include "Display.h"
#include "TaskSync.h"
#include "ui.h"

BLE_HID ble;

static uint8_t keymap[ROWS * COLS] = {
    0x00,      0x00,     0x00,
    KEY_PAUSE, KEY_NEXT, KEY_PLAY_PAUSE  // bottom row: pause timer, next task, play/pause media
};

MacroPad keypad(ble, keymap, ROWS, COLS, ROW_PINS, COL_PINS);
Encoder  encoder(ble, PIN_ENCODER_A, PIN_ENCODER_B);

// ----- Pomodoro state -----
static size_t        currentPhase       = 0;
static int           remainingSeconds   = PHASES[0].minutes * 60;
static bool          timerRunning       = false;
static bool          waitingForContinue = false;  // phase ended, waiting for key to advance
static unsigned long lastSecondTick     = 0;

// ----- Tasks -----
static char   tasks[MAX_TASKS][TASK_MAX_LEN] = {
    "grasp map", "test", "orca handoff", "joint map*", "teleop"
};
static size_t taskCount    = 5;
static size_t taskOffset   = 0;  // first visible task in the queue
static size_t taskSelected = 0;  // highlighted row (0 = top)

static bool timerDirty = true;  // UI needs a timer redraw
static bool tasksDirty = true;  // UI needs a task list redraw

// work phases show the task list; rest phases show a blank screen
static bool isWorkPhase(size_t idx) {
    return strcmp(PHASES[idx].label, "work") == 0;
}

// parse newline-separated task names from a BLE payload, lowercase them for the font
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

// move to next pomodoro phase and reset the countdown
static void advancePhase() {
    currentPhase     = (currentPhase + 1) % PHASE_COUNT;
    remainingSeconds = PHASES[currentPhase].minutes * 60;
    timerDirty = true;
    tasksDirty = true;
}

// count down one second per wall-clock second; stop and wait when time runs out
static void handleTimer() {
    if (!timerRunning) return;
    unsigned long now = millis();
    if (lastSecondTick == 0) { lastSecondTick = now; return; }  // first call: anchor the clock
    if (now - lastSecondTick >= 1000) {
        lastSecondTick += 1000;
        if (remainingSeconds > 0) { remainingSeconds--; timerDirty = true; }
        if (remainingSeconds == 0) {
            timerRunning       = false;
            waitingForContinue = true;
        }
    }
}

// returns true if the key was handled here (prevents MacroPad from forwarding it over BLE)
static bool handleKeyEvent(uint8_t key, bool pressed) {
    if (key == KEY_PAUSE && pressed) {
        if (waitingForContinue) {
            bool wasRest       = !isWorkPhase(currentPhase);
            waitingForContinue = false;
            advancePhase();
            if (wasRest && taskCount > 1 && taskOffset < taskCount - 1) {
                taskOffset++;    // after a rest, surface the next task automatically
                taskSelected = 0;
            }
            lastSecondTick = 0;
            timerRunning   = true;
        } else {
            timerRunning = !timerRunning;
            timerDirty   = true;
        }
        return true;
    }

    if (key == KEY_NEXT && pressed && taskCount > 0 && taskOffset < taskCount - 1) {
        taskOffset++;
        taskSelected = 0;
        tasksDirty   = true;
        return true;
    }

    return false;
}

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
    updateTaskQueue(tasks, taskCount, taskOffset, taskSelected);
}

void loop() {
    keypad.handleKeypad();

    encoder.handle();
    int delta = encoder.consumeDelta();
    while (delta > 0) { ble.sendMediaKey(BLE_HID::specialCodeToMediaCode(KEY_VOL_UP));   delta--; }
    while (delta < 0) { ble.sendMediaKey(BLE_HID::specialCodeToMediaCode(KEY_VOL_DOWN)); delta++; }

    handleTimer();

    static char payload[MAX_TASKS * TASK_MAX_LEN + 32];
    if (getNextTaskPayload(payload, sizeof(payload))) applyTasks(payload);

    if (timerDirty) {
        updateTimer(remainingSeconds, isWorkPhase(currentPhase), waitingForContinue);
        timerDirty = false;
    }
    if (tasksDirty) {
        updateTaskQueue(tasks, taskCount, taskOffset, taskSelected);
        tasksDirty = false;
    }

    // feed LVGL the elapsed time so its animations and timers run correctly
    static unsigned long lastTick = 0;
    unsigned long now = millis();
    if (lastTick == 0) lastTick = now;
    lv_tick_inc(now - lastTick);
    lastTick = now;
    lv_timer_handler();
    delay(5);
}
