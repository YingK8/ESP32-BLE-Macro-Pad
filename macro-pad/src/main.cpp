#include <Arduino.h>

#include "BLE_HID.h"
#include "MacroPad.h"
#include "Rotary_Encoder.h"
#include "Config.h"

BLE_HID ble;

// Rows driven LOW one at a time; cols read with INPUT_PULLUP. Index = r*COLS + c.
//                  col0=GPIO4    col1=GPIO21    col2=GPIO8
//   row0=GPIO1:    [SCREENSHOT]  [SCREEN REC]   [TERMINAL]
//   row1=GPIO5:    [PAUSE]       [NEXT]         [PLAY_PAUSE]
static Key keymap[ROWS * COLS] = {
    // GNOME shortcuts: Print = screenshot UI, Ctrl+Shift+Alt+R = screencast toggle, Ctrl+Alt+T = terminal
    Key::combo(0, HidKey::PRINT_SCREEN),
    Key::combo(Mod::LCTRL | Mod::LSHIFT | Mod::LALT, BLE_HID::charToHidCode('r')),
    Key::combo(Mod::LCTRL | Mod::LALT,               BLE_HID::charToHidCode('t')),
    Key::app(AppKey::PAUSE), Key::app(AppKey::NEXT),  Key::media(MediaKey::PLAY_PAUSE)
};

MacroPad keypad(ble, keymap, ROWS, COLS, ROW_PINS, COL_PINS);
Encoder  encoder(ble, PIN_ENCODER_A, PIN_ENCODER_B);

void setup() {
    Serial.begin(115200);
    ble.begin("ESP32 MacroPad");
    ble.startAdvertising();  // last: NimBLE freezes the GATT table once the server starts
    keypad.begin();
    encoder.begin();
}

void loop() {
    ble.tick();  // flush any deferred media-key release from the previous iteration
    keypad.handleKeypad();

    encoder.handle();
    int delta = encoder.consumeDelta();
    while (delta < 0) { ble.sendMediaKey(MediaKey::VOL_UP);   delta++; }
    while (delta > 0) { ble.sendMediaKey(MediaKey::VOL_DOWN); delta--; }

    delay(5);
}
