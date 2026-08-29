#include "Rotary_Encoder.h"

Encoder* Encoder::_instance = nullptr;

Encoder::Encoder(BLE_HID& ble, uint8_t pinA, uint8_t pinB)
    : _ble(ble),
      _encoder(pinA, pinB, RotaryEncoder::LatchMode::FOUR3),  // 1 count per detent on EC11-style encoders (TWO03 gave 2)
      _pinA(pinA), _pinB(pinB),
      _oldPos(0)
{}

// Called on every edge of pin A or B — keeps the quadrature state machine current
// even when the main loop is blocked (e.g. inside sendMediaKey's delay)
void IRAM_ATTR Encoder::isr() {
    _instance->_encoder.tick();
}

void Encoder::begin() {
    _instance = this;
    _encoder.setPosition(0);
    attachInterrupt(digitalPinToInterrupt(_pinA), isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(_pinB), isr, CHANGE);
}

// handle() no longer calls tick() — the ISR does that on every hardware edge
void Encoder::handle() {
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
                if (_buttonState) _clickEvent = true;
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
