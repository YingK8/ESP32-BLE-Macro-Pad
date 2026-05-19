#include "MacroPad.h"

MacroPad::MacroPad(
    BLE_HID&       ble,
    const uint8_t* keymap,
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
                    uint8_t key = _keys[i];
                    bool isMedia = (key == KEY_PLAY_PAUSE ||
                                    key == KEY_VOL_UP     ||
                                    key == KEY_VOL_DOWN);

                    bool consumed = _handler ? _handler(i, key, current) : false;
                    if (!consumed) {
                        if (current) {
                            if (isMedia) {
                                _ble.sendMediaKey(BLE_HID::specialCodeToMediaCode(key));
                            } else {
                                _ble.sendKey((char)key, true);
                            }
                        } else if (!isMedia) {
                            _ble.sendKey((char)key, false);
                        }
                    }
                }
            }
            _lastButtonState[i] = current;
        }

        digitalWrite(_rowPins[r], HIGH);
        delay(1);
    }
}
