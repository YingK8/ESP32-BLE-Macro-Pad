#include "Encoder.h"

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
        if (newPos < _oldPos) {
            _ble.sendMediaKey(0xE9);  // Volume Up
        } else {
            _ble.sendMediaKey(0xEA);  // Volume Down
        }
        _oldPos = newPos;
    }
}