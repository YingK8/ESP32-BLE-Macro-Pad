#include "ui.h"
#include <lvgl.h>

#define TIMER_H      100  // px: height of the timer band at the top
#define TASK_Y_OFFS   0  // px: gap between timer band and first task row
#define TASK_ROW_H    38  // px: height per task row (= monogram_lower_40 line height)
#define TASK_ROWS      5

extern const lv_font_t monogram_lower_counter_125;
extern const lv_font_t monogram_lower_40;

static lv_obj_t* timerLabel           = nullptr;
static lv_obj_t* highlightBar         = nullptr;  // white strip drawn behind selected row
static lv_obj_t* taskLabels[TASK_ROWS]= {};
static bool      workMode             = true;

// Format "N name" into buf (max 13 chars + null). Overflow chars replaced with *.
static void fmtTask(char* buf, int num, const char* name) {
    char prefix[5];
    snprintf(prefix, sizeof(prefix), "%d ", num);
    int plen   = (int)strlen(prefix);
    int budget = 13 - plen;
    if ((int)strlen(name) <= budget)
        snprintf(buf, 14, "%s%s", prefix, name);
    else
        snprintf(buf, 14, "%s%.*s*", prefix, budget - 1, name);
}

// Pixel y of the top of task row i.
static inline int rowY(int i) { return TIMER_H + TASK_Y_OFFS + i * TASK_ROW_H; }

void createUI(void) {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // --- Timer label (directly on screen, horizontally centered) ---
    timerLabel = lv_label_create(scr);
    lv_obj_set_style_text_font(timerLabel, &monogram_lower_counter_125, 0);
    lv_obj_set_style_text_color(timerLabel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(timerLabel, TFT_WIDTH);
    lv_obj_set_style_text_align(timerLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(timerLabel, 0, (TIMER_H - 72) / 2);
    lv_label_set_text(timerLabel, "00:00");

    // --- Highlight bar — drawn before labels so labels render on top ---
    highlightBar = lv_obj_create(scr);
    lv_obj_remove_style_all(highlightBar);
    lv_obj_set_size(highlightBar, TFT_WIDTH, TASK_ROW_H);
    lv_obj_set_pos(highlightBar, 0, rowY(0));
    lv_obj_set_style_bg_color(highlightBar, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(highlightBar, LV_OPA_COVER, 0);
    lv_obj_add_flag(highlightBar, LV_OBJ_FLAG_HIDDEN);

    // --- Task labels (directly on screen, no container parent) ---
    for (int i = 0; i < TASK_ROWS; i++) {
        taskLabels[i] = lv_label_create(scr);
        lv_obj_set_style_text_font(taskLabels[i], &monogram_lower_40, 0);
        lv_obj_set_style_text_color(taskLabels[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_pos(taskLabels[i], UI_PADDING, rowY(i) + (TASK_ROW_H - 18) / 2);  // 18 = line_height
        lv_label_set_text(taskLabels[i], "");
        lv_obj_add_flag(taskLabels[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void updateTimer(int remainingSecs, bool isWork, bool waitingContinue) {
    workMode = isWork;
    lv_color_t bg = lv_color_hex(isWork ? 0x000000 : 0xFFFFFF);
    lv_color_t fg = lv_color_hex(isWork ? 0xFFFFFF : 0x000000);
    lv_obj_set_style_bg_color(lv_scr_act(), bg, 0);
    lv_obj_set_style_text_color(timerLabel, fg, 0);

    // Hide task area during rest; updateTaskQueue will restore it in work mode
    if (!isWork) {
        lv_obj_add_flag(highlightBar, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < TASK_ROWS; i++) lv_obj_add_flag(taskLabels[i], LV_OBJ_FLAG_HIDDEN);
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", remainingSecs / 60, remainingSecs % 60);
    lv_label_set_text(timerLabel, buf);
    (void)waitingContinue;
}

void updateTaskQueue(const char tasks[][TASK_MAX_LEN], size_t count,
                     size_t offset, size_t selected) {
    if (!workMode) return;

    // Move highlight bar to the selected row
    lv_obj_set_pos(highlightBar, 0, rowY((int)selected));
    if (count > offset) lv_obj_clear_flag(highlightBar, LV_OBJ_FLAG_HIDDEN);
    else                lv_obj_add_flag(highlightBar,   LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < TASK_ROWS; i++) {
        size_t idx = offset + (size_t)i;
        if (idx >= count) {
            lv_obj_add_flag(taskLabels[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(taskLabels[i], LV_OBJ_FLAG_HIDDEN);

        char buf[14];
        fmtTask(buf, (int)(idx + 1), tasks[idx]);
        lv_label_set_text(taskLabels[i], buf);

        // Selected row gets black text (readable on white bar); others get white
        lv_color_t fg = lv_color_hex(((size_t)i == selected) ? 0x000000 : 0xFFFFFF);
        lv_obj_set_style_text_color(taskLabels[i], fg, 0);
    }
}
