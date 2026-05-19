#include "Rotary_Encoder.h"

Encoder::Encoder(BLE_HID& ble, uint8_t pinA, uint8_t pinB)
    : _ble(ble),
      _encoder(pinA, pinB, RotaryEncoder::LatchMode::TWO03),
      _oldPos(0)
{}

void Encoder::begin() {
    _encoder.setPosition(0);
}

void Encoder::handle() {
    _encoder.tick();
    int newPos = _encoder.getPosition();

    if (newPos != _oldPos) {
        _delta += (newPos - _oldPos);
        _oldPos = newPos;
    }

    if (_buttonPin != 0xFF) {
        bool current = (digitalRead(_buttonPin) == LOW);
        if (current != _lastButtonState) {
            _lastDebounceTime = millis();
        }
        if ((millis() - _lastDebounceTime) > DEBOUNCE_DELAY_MS) {
            if (current != _buttonState) {
                _buttonState = current;
                if (_buttonState) {
                    _clickEvent = true;
                }
            }
        }
        _lastButtonState = current;
    }
}

void Encoder::setButtonPin(uint8_t pin) {
    _buttonPin = pin;
    pinMode(_buttonPin, INPUT_PULLUP);
}

int Encoder::consumeDelta() {
    int diff = _delta;
    _delta = 0;
    return diff;
}

bool Encoder::consumeClick() {
    bool clicked = _clickEvent;
    _clickEvent = false;
    return clicked;
}
