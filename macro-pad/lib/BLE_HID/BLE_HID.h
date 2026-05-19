#ifndef BLE_HID_H
#define BLE_HID_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>

// ── Special key codes (values above ASCII range to avoid collisions) ──────────
static constexpr uint8_t KEY_PLAY_PAUSE = 0xF0;
static constexpr uint8_t KEY_VOL_UP     = 0xF1;
static constexpr uint8_t KEY_VOL_DOWN   = 0xF2;

class BLE_HID {
public:
    void begin(const char* deviceName);
    void sendKey(char key, bool pressed);
    void sendMediaKey(uint16_t keyCode);
    bool isConnected() { return _isConnected; }
    void setConnected(bool state) { _isConnected = state; } // public setter, no friend needed
    BLEServer* server() const { return _server; }
    BLEAdvertising* advertising() const { return _advertising; }

    static uint8_t  charToHidCode(char c);
    static uint16_t specialCodeToMediaCode(uint8_t code);

private:
    // BLE HID Objects
    BLEHIDDevice*      _hid           = nullptr;
    BLECharacteristic* _keyboardInput = nullptr;
    BLECharacteristic* _mediaInput    = nullptr;
    bool               _isConnected   = false;
    BLEServer*         _server        = nullptr;
    BLEAdvertising*    _advertising   = nullptr;
};

#endif
