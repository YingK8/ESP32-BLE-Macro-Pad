#include <Arduino.h>

#include "BLE_HID.h"
#include "Config.h"
#include "HostLink.h"
#include "MacroPad.h"
#include "Rotary_Encoder.h"
#include "Screen.h"

BLE_HID  ble;
Screen   screen;
HostLink host;

// Rows driven LOW one at a time; cols read with INPUT_PULLUP. Index = r*COLS + c.
//                  col0=GPIO4    col1=GPIO21    col2=GPIO8
//   row0=GPIO1:    [PREV APP]    [SCREENSHOT]   [NEXT APP]
//   row1=GPIO5:    [ACTION A]    [SPOTLIGHT]    [ACTION B]
//
// Col 1 is GPIO21 = UART0 TX, which the peripheral may hold HIGH; the two keys
// there are the expendable ones for that reason. The four host keys sit on the
// columns known to scan reliably.
static Key keymap[ROWS * COLS] = {
    Key::app(AppKey::PREV),
    Key::combo(Mod::LGUI | Mod::LSHIFT, BLE_HID::charToHidCode('4')),  // macOS: capture selection
    Key::app(AppKey::NEXT),

    Key::app(AppKey::ACTION_A),
    Key::combo(Mod::LGUI, BLE_HID::charToHidCode(' ')),                // macOS: Spotlight
    Key::app(AppKey::ACTION_B)
};

MacroPad keypad(ble, keymap, ROWS, COLS, ROW_PINS, COL_PINS);
Encoder  encoder(ble, PIN_ENCODER_A, PIN_ENCODER_B);

// Set by each frame: when true the host wants encoder deltas instead of HID volume.
static bool encoderToHost = false;

// Last fallback message drawn, or nullptr while a host frame is on screen. Both
// messages are string literals, so comparing pointers is enough to detect a
// change — without this the fallback would repaint on every one of the ~200 loop
// iterations per second, and would never notice BLE connecting.
static const char* statusShown = nullptr;

// Runs before MacroPad's own HID dispatch. Returning true consumes the key, so
// AppKey entries never reach the BLE path — they are ours alone.
static bool onKeyEvent(const Key& key, bool pressed) {
    if (key.type != KeyType::APP) return false;
    screen.noteActivity();
    host.sendKey(key._app, pressed);
    return true;
}

void setup() {
    Serial.begin(115200);
    host.begin();
    screen.begin();
    screen.showStatus("MACROPAD", "waiting for host...");

    ble.begin("ESP32 MacroPad");
    ble.startAdvertising();  // last: NimBLE freezes the GATT table once the server starts
    keypad.begin();
    keypad.setKeyHandler(onKeyEvent);
    encoder.begin();
}

void loop() {
    ble.tick();  // flush any deferred media-key release from the previous iteration

    JsonDocument frame;
    if (host.poll(frame)) {
        encoderToHost = frame["e"].as<int>() != 0;
        screen.drawFrame(frame["l"].as<JsonArrayConst>());
        statusShown = nullptr;
    } else if (!host.hostPresent()) {
        const char* detail = ble.isConnected() ? "BLE ok - no host app"
                                               : "waiting for host...";
        if (detail != statusShown) {
            screen.showStatus("MACROPAD", detail);
            statusShown = detail;
        }
    }

    keypad.handleKeypad();

    encoder.handle();
    int delta = encoder.consumeDelta();
    if (delta != 0) {
        screen.noteActivity();
        if (encoderToHost) {
            // Flip the sign here if scrolling on the pad feels backwards.
            host.sendEncoder(delta);
        } else {
            while (delta < 0) { ble.sendMediaKey(MediaKey::VOL_UP);   delta++; }
            while (delta > 0) { ble.sendMediaKey(MediaKey::VOL_DOWN); delta--; }
        }
    }

    screen.tick();
    delay(5);
}
