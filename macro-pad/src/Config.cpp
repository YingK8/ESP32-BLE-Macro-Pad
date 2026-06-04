#include "Config.h"

// Single definition of all array constants — including this header in multiple
// .cpp files would otherwise create one copy per translation unit.

const uint8_t ROW_PINS[ROWS] = {1, 5};
const uint8_t COL_PINS[COLS] = {4, 21, 8};

const Phase PHASES[PHASE_COUNT] = {
    {"WORK",      30, true },
    {"REST",       5, false},
    {"WORK",      30, true },
    {"REST",       5, false},
    {"WORK",      30, true },
    {"REST",       5, false},
    {"WORK",      30, true },
    {"LONG REST", 35, true },
};
