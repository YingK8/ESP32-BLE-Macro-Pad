#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>
#include <RotaryEncoder.h>
#include "BLE_HID.h"

class Encoder {
public:
    Encoder(BLE_HID& ble, uint8_t pinA, uint8_t pinB);
    void begin();
    void handle();
    void setButtonPin(uint8_t pin);
    int consumeDelta();
    bool consumeClick();

private:
    BLE_HID& _ble;
    RotaryEncoder _encoder;
    int _oldPos;
    int _delta = 0;
    uint8_t _buttonPin = 0xFF;
    bool _buttonState = false;
    bool _lastButtonState = false;
    unsigned long _lastDebounceTime = 0;
    bool _clickEvent = false;
    static constexpr unsigned long DEBOUNCE_DELAY_MS = 50;
};

#endif
