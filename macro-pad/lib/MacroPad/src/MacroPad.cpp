#include "MacroPad.h"

MacroPad::MacroPad(
    BLE_HID&       ble,
    const Key*     keymap,
    uint8_t        rows,
    uint8_t        cols,
    const uint8_t* rowPins,
    const uint8_t* colPins
)
    : _ble(ble), _rows(rows), _cols(cols)
{
    if (_rows > MAX_ROWS) _rows = MAX_ROWS;
    if (_cols > MAX_COLS) _cols = MAX_COLS;

    for (uint8_t r = 0; r < _rows; r++) {
        _rowPins[r] = rowPins[r];
        for (uint8_t c = 0; c < _cols; c++) {
            int i = r * _cols + c;
            _keys[i]             = keymap[i];
            _buttonState[i]      = false;
            _lastButtonState[i]  = false;
            _lastDebounceTime[i] = 0;
        }
    }
    for (uint8_t c = 0; c < _cols; c++) {
        _colPins[c] = colPins[c];
    }
}

void MacroPad::begin() {
    for (uint8_t r = 0; r < _rows; r++) {
        pinMode(_rowPins[r], OUTPUT);
        digitalWrite(_rowPins[r], HIGH);
    }
    for (uint8_t c = 0; c < _cols; c++) {
        pinMode(_colPins[c], INPUT_PULLUP);
    }
}

void MacroPad::handleKeypad() {
    for (uint8_t r = 0; r < _rows; r++) {
        digitalWrite(_rowPins[r], LOW);
        delayMicroseconds(10);

        for (uint8_t c = 0; c < _cols; c++) {
            int i = r * _cols + c;
            bool current = (digitalRead(_colPins[c]) == LOW);

            if (current != _lastButtonState[i]) {
                _lastDebounceTime[i] = millis();
            }

            if ((millis() - _lastDebounceTime[i]) > DEBOUNCE_DELAY_MS) {
                if (current != _buttonState[i]) {
                    _buttonState[i] = current;
                    const Key& key = _keys[i];

                    bool consumed = _handler ? _handler(key, current) : false;
                    if (!consumed) {
                        if (current) {
                            switch (key.type) {
                                case KeyType::CHAR:
                                    _ble.sendKey(key._ch, true);
                                    break;
                                case KeyType::MEDIA:
                                    _ble.sendMediaKey(key._media);
                                    break;
                                case KeyType::STRING:
                                    // type each character as a press+release pair
                                    for (const char* p = key._str; *p; p++) {
                                        _ble.sendKey(*p, true);
                                        _ble.sendKey(*p, false);
                                    }
                                    break;
                                default:
                                    break;
                            }
                        } else if (key.type == KeyType::CHAR) {
                            _ble.sendKey(key._ch, false);  // key-up for held chars
                        }
                    }
                }
            }
            _lastButtonState[i] = current;
        }

        digitalWrite(_rowPins[r], HIGH);
    }
}
