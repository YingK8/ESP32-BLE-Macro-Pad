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
    void sendKey(char key, bool pressed);
    void sendMediaKey(uint16_t keyCode);  // pass a MediaKey:: constant directly
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
};

#endif
