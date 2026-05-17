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

private:
    BLE_HID& _ble;
    RotaryEncoder _encoder;
    int _oldPos;
};

#endif