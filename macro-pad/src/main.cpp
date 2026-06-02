#include <Arduino.h>
#include <lvgl.h>

#include "BLE_HID.h"
#include "MacroPad.h"
#include "Rotary_Encoder.h"
#include "Config.h"
#include "Display.h"
#include "TaskSync.h"
#include "ui.h"

// ── Burn-in / sleep state ─────────────────────────────────────────────────────
static unsigned long lastUserActivityMs = 0;

// Two-phase sleep model:
//   isDimmingDown — fade is in progress; input cancels the fade (no swallow)
//   isDisplayDimmed — fade finished, screen fully dark; input wakes + swallows key
static bool          isDimmingDown    = false;
static bool          isDisplayDimmed  = false;
static bool          isFadingToSleep  = false;  // dim→off fade after SLEEP_TIMEOUT_MS
static unsigned long dimmedSinceMs    = 0;       // when we entered the dim state
static bool          swallowNextKey   = false;

// Returns true if the display was sleeping (dim or off) and just woke.
// Key-event callers set swallowNextKey on true; encoder/BLE callers ignore it.
static bool markActivity() {
    lastUserActivityMs = millis();
    if (isDisplayDimmed) {
        isDisplayDimmed = false;
        isFadingToSleep = false;
        setSleepMode(false);
        cancelFade(BRIGHTNESS);  // fade up from wherever brightness currently is
        return true;
    } else if (isDimmingDown) {
        isDimmingDown = false;
        setSleepMode(false);
        cancelFade(BRIGHTNESS);
    }
    return false;
}

BLE_HID ble;

// Physical layout (2R × 3C), col pins = {GPIO4, GPIO21, GPIO8}:
//   col:      0        1(DEAD)  2
//   row 0:  [none]   [none]   [none]       GPIO1 × col  (row untested)
//   row 1:  [PAUSE]  [none]   [NEXT]       GPIO5 × col  (row confirmed working)
//
// GPIO21 (col 1) = UART0 TX, permanently dead for input.
// NEXT moved to (r1,c2) = GPIO5 × GPIO8 — both confirmed working.
// Volume is handled by the encoder, so PLAY_PAUSE hardware button is unused.
static Key keymap[ROWS * COLS] = {
    Key::none(),             Key::none(),  Key::none(),
    Key::app(AppKey::PAUSE), Key::none(),  Key::app(AppKey::NEXT)
};

MacroPad keypad(ble, keymap, ROWS, COLS, ROW_PINS, COL_PINS);
Encoder  encoder(ble, PIN_ENCODER_A, PIN_ENCODER_B);

// ----- Pomodoro state -----
static size_t        currentPhase       = 0;
static int           remainingSeconds   = 0;  // set in setup() from PHASES[currentPhase]
static bool          timerRunning       = false;
static bool          waitingForContinue = false;  // phase ended, waiting for key to advance
static unsigned long lastSecondTick     = 0;
static int           completedSessions  = 4;      // work phases finished; drives break-screen dots

// ----- Tasks -----
static char   tasks[MAX_TASKS][TASK_MAX_LEN] = {
    "GRASP MAP", "TEST", "ORCA HANDOFF", "JOINT MAP*", "TELEOP"
};
static size_t taskCount    = 5;
static size_t taskOffset   = 0;  // first visible task in the queue
static size_t taskSelected = 0;  // highlighted row (0 = top)

static bool timerDirty = true;  // UI needs a timer redraw
static bool tasksDirty = true;  // UI needs a task list redraw

// work phases show the task list; rest phases show a blank screen
static bool isWorkPhase(size_t idx) {
    return strcmp(PHASES[idx].label, "WORK") == 0;
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
                if (*q >= 'a' && *q <= 'z') *q -= 32;
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
    if (isWorkPhase(currentPhase)) completedSessions++;  // count the session we're leaving
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
static bool handleKeyEvent(const Key& key, bool pressed) {
    if (key.type != KeyType::APP) return false;

    // If this key woke the display, arm the swallow so the action doesn't also fire.
    // Encoder wakes don't go through here, so they never accidentally arm the swallow.
    if (markActivity()) swallowNextKey = true;

    if (swallowNextKey) {
        if (!pressed) swallowNextKey = false;  // clear on release so next press is normal
        return true;
    }

    if (key._app == AppKey::PAUSE && pressed) {
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
            if (timerRunning) lastSecondTick = 0;  // re-anchor clock on unpause
            timerDirty   = true;
        }
        return true;
    }

    if (key._app == AppKey::NEXT && pressed) {
        if (taskCount > 0 && taskOffset < taskCount - 1) {
            taskOffset++;
            taskSelected = 0;
            tasksDirty   = true;
        }
        return true;
    }

    return false;
}

void setup() {
    remainingSeconds = PHASES[currentPhase].minutes * 60;  // derive from whichever phase is set
    Serial.begin(115200);
    ble.begin("ESP32 MacroPad");
    setupTaskService(ble);
    keypad.begin();
    keypad.setKeyHandler(handleKeyEvent);
    encoder.begin();
    setupDisplay();
    createUI();
    updateTimer(remainingSeconds, PHASES[currentPhase].label, timerRunning, waitingForContinue, completedSessions);
    updateTaskQueue(tasks, taskCount, taskOffset, taskSelected);
}

void loop() {
    keypad.handleKeypad();

    encoder.handle();
    int delta = encoder.consumeDelta();
    if (delta != 0) markActivity();  // encoder turn = user input
    while (delta < 0) { ble.sendMediaKey(MediaKey::VOL_UP);   delta++; }
    while (delta > 0) { ble.sendMediaKey(MediaKey::VOL_DOWN); delta--; }

    handleTimer();

    // ── Column 2 (GPIO8) pin debug ────────────────────────────────────────────
    // Drives each row LOW in turn and reads GPIO8 analog + digital.
    // With INPUT_PULLUP and no key pressed: analog ~4095, digital HIGH.
    // With key pressed (row driven LOW through button): analog ~0, digital LOW.
    // Prints every 300 ms so the serial monitor stays readable.
    {
        static unsigned long lastDbg = 0;
        if (millis() - lastDbg > 300) {
            lastDbg = millis();
            pinMode(8, INPUT_PULLUP);

            // Row 0 = GPIO1: drive LOW, read col 2 (GPIO8)
            pinMode(1, OUTPUT); digitalWrite(1, LOW); delayMicroseconds(10);
            int r0 = digitalRead(8);  // LOW(0)=pressed, HIGH(1)=open
            digitalWrite(1, HIGH);

            // Row 1 = GPIO5: drive LOW, read col 2 (GPIO8)
            pinMode(5, OUTPUT); digitalWrite(5, LOW); delayMicroseconds(10);
            int r1 = digitalRead(8);
            digitalWrite(5, HIGH);

            Serial.printf("[COL2] row0(GPIO1)=%d row1(GPIO5)=%d  (0=pressed 1=open)\n", r0, r1);
        }
    }

    static char payload[MAX_TASKS * TASK_MAX_LEN + 32];
    if (getNextTaskPayload(payload, sizeof(payload))) { applyTasks(payload); markActivity(); }

    if (timerDirty) {
        updateTimer(remainingSeconds, PHASES[currentPhase].label, timerRunning, waitingForContinue, completedSessions);
        timerDirty = false;
    }
    if (tasksDirty) {
        updateTaskQueue(tasks, taskCount, taskOffset, taskSelected);
        tasksDirty = false;
    }

    // feed LVGL the elapsed time so its animations and timers run correctly
    static unsigned long lastTick  = 0;
    static unsigned long lastShift = 0;
    unsigned long now = millis();
    if (lastTick  == 0) lastTick  = now;
    if (lastShift == 0) lastShift = now;

    // Start the fade-down once after DIM_TIMEOUT_MS of inactivity.
    if (!isDisplayDimmed && !isDimmingDown && (now - lastUserActivityMs) > DIM_TIMEOUT_MS) {
        isDimmingDown = true;
        setSleepMode(true);                           // switch to minimal centered UI
        startFade(BRIGHTNESS, DIM_BRIGHTNESS, 2000);  // 2 s fade-down
    }

    // When the fade-down completes, enter the dim state and start the sleep clock.
    bool stillFading = tickFade();
    if (isDimmingDown && !stillFading) {
        isDimmingDown   = false;
        isDisplayDimmed = true;
        dimmedSinceMs   = now;
    }

    // After SLEEP_TIMEOUT_MS in the dim state, fade to fully off.
    // isFadingToSleep prevents re-triggering once the fade has started.
    if (isDisplayDimmed && !isFadingToSleep && (now - dimmedSinceMs) > SLEEP_TIMEOUT_MS) {
        isFadingToSleep = true;
        startFade(DIM_BRIGHTNESS, 0, 2000);
    }

    // Pixel shift: nudge the label every SHIFT_INTERVAL_MS to distribute stress.
    if (now - lastShift >= SHIFT_INTERVAL_MS) {
        lastShift = now;
        shiftPixels();
    }

    lv_tick_inc(now - lastTick);
    lastTick = now;
    lv_timer_handler();
    delay(5);
}
