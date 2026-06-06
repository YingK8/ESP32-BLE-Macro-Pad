#ifndef BLE_HID_H
#define BLE_HID_H

#include <Arduino.h>
#include <NimBLEDevice.h>     // replaces all Bluedroid BLE*.h; CCCDs are auto-created (no BLE2902)
#include <NimBLEHIDDevice.h>  // HID helper isn't pulled in by NimBLEDevice.h

// HID Consumer Page codes — used directly in Key::media(), no lookup needed
namespace MediaKey {
    static constexpr uint16_t PLAY_PAUSE = 0x00CD;
    static constexpr uint16_t VOL_UP     = 0x00E9;
    static constexpr uint16_t VOL_DOWN   = 0x00EA;
}

// HID modifier bitmap (report byte 0) — OR these together for multi-mod shortcuts
namespace Mod {
    static constexpr uint8_t LCTRL  = 0x01;
    static constexpr uint8_t LSHIFT = 0x02;
    static constexpr uint8_t LALT   = 0x04;
    static constexpr uint8_t LGUI   = 0x08;  // Super/Win/Cmd
}

// HID Keyboard Page codes that charToHidCode() can't express (non-printable keys)
namespace HidKey {
    static constexpr uint8_t PRINT_SCREEN = 0x46;
}

class BLE_HID {
public:
    void begin(const char* deviceName);
    void startAdvertising();              // call from setup() AFTER all services are registered
    void tick();                          // call every loop() — flushes pending media-key release
    void sendKey(char key, bool pressed);
    void sendMediaKey(uint16_t keyCode);  // pass a MediaKey:: constant directly; non-blocking
    void sendCombo(uint8_t mods, uint8_t hidCode, bool pressed);  // modifier shortcut, e.g. Ctrl+Alt+T
    bool isConnected() { return _isConnected; }
    void setConnected(bool state) { _isConnected = state; }
    NimBLEServer* server() const { return _server; }
    NimBLEAdvertising* advertising() const { return _advertising; }

    static uint8_t charToHidCode(char c);  // maps printable ASCII to USB HID key codes

private:
    NimBLEHIDDevice*      _hid           = nullptr;
    NimBLECharacteristic* _keyboardInput = nullptr;
    NimBLECharacteristic* _mediaInput    = nullptr;
    bool                  _isConnected   = false;
    NimBLEServer*         _server        = nullptr;
    NimBLEAdvertising*    _advertising   = nullptr;

    // Non-blocking media-key release: press is sent immediately, release is sent
    // by tick() ~MEDIA_RELEASE_MS later. Removes the 50ms delay that was stalling loop().
    bool               _mediaReleasePending = false;
    unsigned long      _mediaPressedAtMs    = 0;
    static constexpr unsigned long MEDIA_RELEASE_MS = 20;
};

#endif
