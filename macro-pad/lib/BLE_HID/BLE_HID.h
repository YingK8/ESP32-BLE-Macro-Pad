#ifndef BLE_HID_H
#define BLE_HID_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>

// HID Consumer Page codes — used directly in Key::media(), no lookup needed
namespace MediaKey {
    static constexpr uint16_t PLAY_PAUSE = 0x00CD;
    static constexpr uint16_t VOL_UP     = 0x00E9;
    static constexpr uint16_t VOL_DOWN   = 0x00EA;
}

class BLE_HID {
public:
    void begin(const char* deviceName);
    void tick();                          // call every loop() — flushes pending media-key release
    void sendKey(char key, bool pressed);
    void sendMediaKey(uint16_t keyCode);  // pass a MediaKey:: constant directly; non-blocking
    bool isConnected() { return _isConnected; }
    void setConnected(bool state) { _isConnected = state; }
    BLEServer* server() const { return _server; }
    BLEAdvertising* advertising() const { return _advertising; }

    static uint8_t charToHidCode(char c);  // maps printable ASCII to USB HID key codes

private:
    BLEHIDDevice*      _hid           = nullptr;
    BLECharacteristic* _keyboardInput = nullptr;
    BLECharacteristic* _mediaInput    = nullptr;
    bool               _isConnected   = false;
    BLEServer*         _server        = nullptr;
    BLEAdvertising*    _advertising   = nullptr;

    // Non-blocking media-key release: press is sent immediately, release is sent
    // by tick() ~MEDIA_RELEASE_MS later. Removes the 50ms delay that was stalling loop().
    bool               _mediaReleasePending = false;
    unsigned long      _mediaPressedAtMs    = 0;
    static constexpr unsigned long MEDIA_RELEASE_MS = 20;
};

#endif
