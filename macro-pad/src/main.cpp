#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#include "BLE_HID.h"
#include "MacroPad.h"
#include "Rotary_Encoder.h"
#include "Config.h"

// ----- BLE + input devices -----
BLE_HID ble;

static uint8_t keymap[ROWS * COLS] = {
    0x00, 'b', 0x00,
    'd',  'e', 'f'
};

MacroPad keypad(ble, keymap, ROWS, COLS, ROW_PINS, COL_PINS);
Encoder encoder(ble, PIN_ENCODER_A, PIN_ENCODER_B);

// ----- Display + LVGL -----
static Arduino_DataBus* gfxBus = nullptr;
static Arduino_GFX* gfx = nullptr;
static lv_disp_draw_buf_t drawBuf;
static lv_color_t buf1[TFT_WIDTH * 20];
static lv_color_t buf2[TFT_WIDTH * 20];

static lv_obj_t* timerLabel = nullptr;
static lv_obj_t* statusLabel = nullptr;
static lv_obj_t* taskContainer = nullptr;
static lv_obj_t* taskLabels[MAX_TASKS] = {};
static lv_obj_t* noTasksLabel = nullptr;
static lv_obj_t* dotContainer = nullptr;

struct Phase {
    const char* label;
    uint16_t minutes;
    bool bigDot;
};

static constexpr Phase PHASES[] = {
    {"Work", 30, true},
    {"Break", 5, false},
    {"Work", 30, true},
    {"Break", 5, false},
    {"Work", 30, true},
    {"Break", 5, false},
    {"Work", 30, true},
    {"Long Break", 35, true}
};

static constexpr size_t PHASE_COUNT = sizeof(PHASES) / sizeof(PHASES[0]);
static lv_obj_t* phaseDots[PHASE_COUNT] = {};

// ----- Pomodoro state -----
static size_t currentPhase = 0;
static int remainingSeconds = PHASES[0].minutes * 60;
static bool timerRunning = false;
static unsigned long lastSecondTick = 0;

// ----- Tasks -----
static String tasks[MAX_TASKS];
static bool taskDone[MAX_TASKS] = {};
static size_t taskCount = 0;
static int selectedTask = 0;
static bool tasksDirty = true;
static bool timerDirty = true;
static bool dotsDirty = true;

// ----- BLE task sync -----
static const char* TASK_SERVICE_UUID = "c3a7b7a0-3c1b-4d46-9f5c-9f0d9d1a9d01";
static const char* TASK_CHAR_UUID = "c3a7b7a0-3c1b-4d46-9f5c-9f0d9d1a9d02";
static const char* TASK_TERMINATOR = "--END--";

static portMUX_TYPE taskMux = portMUX_INITIALIZER_UNLOCKED;
static String pendingTaskPayload;
static volatile bool tasksPending = false;

static void queueTaskPayload(const String& payload) {
    portENTER_CRITICAL(&taskMux);
    pendingTaskPayload = payload;
    tasksPending = true;
    portEXIT_CRITICAL(&taskMux);
}

class TaskCharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic* characteristic) override {
        String value = characteristic->getValue();
        if (value.isEmpty()) {
            return;
        }

        _buffer += value;
        int endIndex = _buffer.indexOf(TASK_TERMINATOR);
        if (endIndex >= 0) {
            String payload = _buffer.substring(0, endIndex);
            _buffer = "";
            queueTaskPayload(payload);
        }
    }

private:
    String _buffer;
};

static void setupTaskService() {
    BLEServer* server = ble.server();
    if (!server) {
        return;
    }

    BLEService* taskService = server->createService(TASK_SERVICE_UUID);
    BLECharacteristic* taskChar = taskService->createCharacteristic(
        TASK_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    taskChar->setCallbacks(new TaskCharacteristicCallbacks());
    taskService->start();

    BLEAdvertising* adv = ble.advertising();
    if (adv) {
        adv->addServiceUUID(TASK_SERVICE_UUID);
        adv->start();
    }
}

// ----- LVGL glue -----
static void lvglFlush(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    gfx->draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t*>(color_p), w, h);
    lv_disp_flush_ready(disp);
}

static void setupDisplay() {
    gfxBus = new Arduino_ESP32SPI(PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_SCK, PIN_TFT_MOSI, PIN_TFT_MISO);
    gfx = new Arduino_ST7789(gfxBus, PIN_TFT_RST, TFT_ROTATION, true, TFT_WIDTH, TFT_HEIGHT, TFT_OFFSET_X, TFT_OFFSET_Y);
    gfx->begin();
    if (PIN_TFT_BL >= 0) {
        pinMode(PIN_TFT_BL, OUTPUT);
        digitalWrite(PIN_TFT_BL, HIGH);
    }

    lv_init();
    lv_disp_draw_buf_init(&drawBuf, buf1, buf2, TFT_WIDTH * 20);

    static lv_disp_drv_t dispDrv;
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res = TFT_WIDTH;
    dispDrv.ver_res = TFT_HEIGHT;
    dispDrv.flush_cb = lvglFlush;
    dispDrv.draw_buf = &drawBuf;
    lv_disp_drv_register(&dispDrv);
}

// ----- UI construction -----
static void createDots() {
    dotContainer = lv_obj_create(lv_scr_act());
    lv_obj_set_size(dotContainer, 24, 200);
    lv_obj_align(dotContainer, LV_ALIGN_TOP_RIGHT, -4, 8);
    lv_obj_clear_flag(dotContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(dotContainer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dotContainer, 0, 0);

    for (size_t i = 0; i < PHASE_COUNT; i++) {
        uint8_t size = PHASES[i].bigDot ? 10 : 6;
        lv_obj_t* dot = lv_obj_create(dotContainer);
        lv_obj_set_size(dot, size, size);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(dot, 1, 0);
        lv_obj_set_style_border_color(dot, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(dot, LV_ALIGN_TOP_MID, 0, i * 22);
        phaseDots[i] = dot;
    }
}

static void createTaskList() {
    taskContainer = lv_obj_create(lv_scr_act());
    lv_obj_set_size(taskContainer, 190, 190);
    lv_obj_align(taskContainer, LV_ALIGN_TOP_LEFT, 8, 70);
    lv_obj_clear_flag(taskContainer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(taskContainer, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(taskContainer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(taskContainer, 0, 0);

    noTasksLabel = lv_label_create(taskContainer);
    lv_label_set_text(noTasksLabel, "No tasks");
    lv_obj_set_style_text_font(noTasksLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(noTasksLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(noTasksLabel, LV_ALIGN_TOP_LEFT, 4, 4);

    for (size_t i = 0; i < MAX_TASKS; i++) {
        taskLabels[i] = lv_label_create(taskContainer);
        lv_obj_set_style_text_font(taskLabels[i], &lv_font_montserrat_14, 0);
        lv_obj_align(taskLabels[i], LV_ALIGN_TOP_LEFT, 4, 4 + (i * 16));
        lv_obj_add_flag(taskLabels[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void createUI() {
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

    timerLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(timerLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(timerLabel, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(timerLabel, "30:00");
    lv_obj_align(timerLabel, LV_ALIGN_TOP_LEFT, 8, 6);

    statusLabel = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(statusLabel, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(statusLabel, "Work (Paused)");
    lv_obj_align(statusLabel, LV_ALIGN_TOP_LEFT, 8, 44);

    createTaskList();
    createDots();
}

// ----- UI updates -----
static void updateDots() {
    for (size_t i = 0; i < PHASE_COUNT; i++) {
        lv_color_t color = (i == currentPhase) ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x000000);
        lv_obj_set_style_bg_color(phaseDots[i], color, 0);
    }
}

static void updateTimerUI() {
    const Phase& phase = PHASES[currentPhase];
    char timeBuf[10];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", remainingSeconds / 60, remainingSeconds % 60);
    lv_label_set_text(timerLabel, timeBuf);

    char statusBuf[24];
    if (timerRunning) {
        snprintf(statusBuf, sizeof(statusBuf), "%s", phase.label);
    } else {
        snprintf(statusBuf, sizeof(statusBuf), "%s (Paused)", phase.label);
    }
    lv_label_set_text(statusLabel, statusBuf);
}

static void updateTasksUI() {
    if (taskCount == 0) {
        lv_obj_clear_flag(noTasksLabel, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(noTasksLabel, LV_OBJ_FLAG_HIDDEN);
    }

    for (size_t i = 0; i < MAX_TASKS; i++) {
        if (i < taskCount) {
            char line[64];
            const char* marker = taskDone[i] ? "[x]" : "[ ]";
            const char* cursor = (static_cast<int>(i) == selectedTask) ? ">" : " ";
            snprintf(line, sizeof(line), "%s %s %s", cursor, marker, tasks[i].c_str());
            lv_label_set_text(taskLabels[i], line);
            lv_obj_clear_flag(taskLabels[i], LV_OBJ_FLAG_HIDDEN);

            lv_obj_set_style_bg_opa(taskLabels[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_text_color(taskLabels[i], lv_color_hex(0xFFFFFF), 0);
        } else {
            lv_obj_add_flag(taskLabels[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// ----- Task parsing -----
static void applyTasks(const String& payload) {
    size_t count = 0;
    int start = 0;
    while (start < payload.length() && count < MAX_TASKS) {
        int end = payload.indexOf('\n', start);
        if (end < 0) {
            end = payload.length();
        }
        String line = payload.substring(start, end);
        line.trim();
        if (line.length() > 0) {
            tasks[count] = line;
            taskDone[count] = false;
            count++;
        }
        start = end + 1;
    }
    taskCount = count;
    selectedTask = (taskCount > 0) ? 0 : 0;
    tasksDirty = true;
}

// ----- Pomodoro control -----
static void advancePhase() {
    currentPhase = (currentPhase + 1) % PHASE_COUNT;
    remainingSeconds = PHASES[currentPhase].minutes * 60;
    dotsDirty = true;
    timerDirty = true;
}

static void togglePause() {
    timerRunning = !timerRunning;
    timerDirty = true;
}

static void handleTimer() {
    if (!timerRunning) {
        return;
    }
    unsigned long now = millis();
    if (lastSecondTick == 0) {
        lastSecondTick = now;
        return;
    }
    if (now - lastSecondTick >= 1000) {
        lastSecondTick += 1000;
        if (remainingSeconds > 0) {
            remainingSeconds--;
            timerDirty = true;
        }
        if (remainingSeconds == 0) {
            advancePhase();
        }
    }
}

// ----- Input handling -----
static bool handleKeyEvent(uint8_t index, uint8_t, bool pressed) {
    if (index == PAUSE_KEY_INDEX) {
        if (pressed) {
            togglePause();
        }
        return true;
    }
    if (index == ENCODER_CLICK_INDEX) {
        if (pressed && taskCount > 0) {
            taskDone[selectedTask] = !taskDone[selectedTask];
            tasksDirty = true;
        }
        return true;
    }
    return false;
}

static void handleEncoder() {
    encoder.handle();
    int delta = encoder.consumeDelta();
    if (delta != 0 && taskCount > 0) {
        selectedTask += (delta > 0) ? 1 : -1;
        if (selectedTask < 0) selectedTask = 0;
        if (selectedTask >= static_cast<int>(taskCount)) selectedTask = taskCount - 1;
        tasksDirty = true;
    }
}

void setup() {
    Serial.begin(115200);

    ble.begin("ESP32 MacroPad");
    setupTaskService();

    keypad.begin();
    keypad.setKeyHandler(handleKeyEvent);

    encoder.begin();

    setupDisplay();
    createUI();
    updateTimerUI();
    updateTasksUI();
    updateDots();
}

void loop() {
    keypad.handleKeypad();
    handleEncoder();
    handleTimer();

    if (tasksPending) {
        String payload;
        portENTER_CRITICAL(&taskMux);
        if (tasksPending) {
            payload = pendingTaskPayload;
            tasksPending = false;
        }
        portEXIT_CRITICAL(&taskMux);
        if (payload.length() > 0) {
            applyTasks(payload);
        }
    }

    if (timerDirty) {
        updateTimerUI();
        timerDirty = false;
    }
    if (tasksDirty) {
        updateTasksUI();
        tasksDirty = false;
    }
    if (dotsDirty) {
        updateDots();
        dotsDirty = false;
    }

    static unsigned long lastTick = 0;
    unsigned long now = millis();
    if (lastTick == 0) {
        lastTick = now;
    }
    lv_tick_inc(now - lastTick);
    lastTick = now;
    lv_timer_handler();
    delay(5);
}
