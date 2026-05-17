#ifndef MACRO_PAD_H
#define MACRO_PAD_H

#include <Arduino.h>
#include "BLE_HID.h"

static constexpr uint8_t MAX_ROWS = 8;
static constexpr uint8_t MAX_COLS = 8;
class MacroPad; // Forward declaration for server callbacks

class MacroPadServerCallbacks : public BLEServerCallbacks {
public:
    explicit MacroPadServerCallbacks(MacroPad* pad) : _pad(pad) {}
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;

private:
    MacroPad* _pad;
};

class MacroPad {
public:
    MacroPad(
        BLE_HID& ble,
        const uint8_t* keymap,   // flat array, row-major: keymap[r * width + c]
        uint8_t rows,
        uint8_t cols,
        const uint8_t* rowPins,
        const uint8_t* colPins
    );

    void begin();
    void handleKeypad();
    void sendKey(char key, bool pressed);
    void sendMediaKey(uint16_t keyCode);
    bool connected() const { return _isConnected; }

    static uint8_t  charToHidCode(char c);
    static uint16_t specialCodeToMediaCode(uint8_t code);

private:
    BLE_HID& _ble;

    // Runtime dimensions
    uint8_t _rows;
    uint8_t _cols;

    uint8_t       _rowPins[MAX_ROWS];
    uint8_t       _colPins[MAX_COLS];
    uint8_t       _keys[MAX_ROWS * MAX_COLS];
    bool          _buttonState[MAX_ROWS * MAX_COLS];
    bool          _lastButtonState[MAX_ROWS * MAX_COLS];
    unsigned long _lastDebounceTime[MAX_ROWS * MAX_COLS];

    // BLE
    BLEHIDDevice*      _hid           = nullptr;
    BLECharacteristic* _keyboardInput = nullptr;
    BLECharacteristic* _mediaInput    = nullptr;
    bool               _isConnected   = false;

    static constexpr unsigned long DEBOUNCE_DELAY_MS = 50;

    friend class MacroPadServerCallbacks;
};

#endif // MACRO_PAD_H