#ifndef MACRO_PAD_H
#define MACRO_PAD_H

#include <Arduino.h>
#include "BLE_HID.h"

// ── Key type system ───────────────────────────────────────────────────────────

enum class KeyType : uint8_t { NONE, CHAR, MEDIA, STRING, APP };

struct Key {
    KeyType type = KeyType::NONE;
    union {
        char        _ch;
        uint16_t    _media;
        const char* _str;
        uint8_t     _app;
    };

    static Key none()               { Key k; k.type = KeyType::NONE;               return k; }
    static Key ch(char c)           { Key k; k.type = KeyType::CHAR;   k._ch    = c;    return k; }
    static Key media(uint16_t code) { Key k; k.type = KeyType::MEDIA;  k._media = code; return k; }
    static Key str(const char* s)   { Key k; k.type = KeyType::STRING; k._str   = s;    return k; }
    static Key app(uint8_t code)    { Key k; k.type = KeyType::APP;    k._app   = code; return k; }
};

// App-level key codes — consumed by the KeyEventHandler, never sent over BLE
namespace AppKey {
    static constexpr uint8_t PAUSE = 0x01;
    static constexpr uint8_t NEXT  = 0x02;
    static constexpr uint8_t SKIP  = 0x03;  // reserved
}

// ── MacroPad class ────────────────────────────────────────────────────────────

static constexpr uint8_t MAX_ROWS = 8;
static constexpr uint8_t MAX_COLS = 8;

class MacroPad;
using KeyEventHandler = bool (*)(const Key& key, bool pressed);

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
        const Key* keymap,   // flat array, row-major: keymap[r * cols + c]
        uint8_t rows,
        uint8_t cols,
        const uint8_t* rowPins,
        const uint8_t* colPins
    );

    void begin();
    void handleKeypad();
    void setKeyHandler(KeyEventHandler handler) { _handler = handler; }
    bool connected() const { return _isConnected; }

private:
    BLE_HID& _ble;
    uint8_t  _rows;
    uint8_t  _cols;

    uint8_t       _rowPins[MAX_ROWS];
    uint8_t       _colPins[MAX_COLS];
    Key           _keys[MAX_ROWS * MAX_COLS];
    bool          _buttonState[MAX_ROWS * MAX_COLS];
    bool          _lastButtonState[MAX_ROWS * MAX_COLS];
    unsigned long _lastDebounceTime[MAX_ROWS * MAX_COLS];
    KeyEventHandler _handler = nullptr;

    BLEHIDDevice*      _hid           = nullptr;
    BLECharacteristic* _keyboardInput = nullptr;
    BLECharacteristic* _mediaInput    = nullptr;
    bool               _isConnected   = false;

    static constexpr unsigned long DEBOUNCE_DELAY_MS = 50;

    friend class MacroPadServerCallbacks;
};

#endif // MACRO_PAD_H
