#include <Arduino.h>
#include "Encoder.h"
#include "MacroPad.h"

BLE_HID ble;

// --- Keypad Configuration ---
const byte ROWS = 2;
const byte COLS = 3;

uint8_t keys[ROWS *COLS] = {
  'a', 'b', KEY_PLAY_PAUSE, 
  'd', 'e', 'f'
};

byte rowPins[ROWS] = {5, 1};
byte colPins[COLS] = {4, 20, 8};

MacroPad keypad(keys, ROWS, COLS, rowPins, colPins);

Encoder enc(ble, 9, 21);

void setup() {
    Serial.begin(115200);
    ble.begin("ESP32 MacroPad");
    pad.begin();
    enc.begin();
}

void loop() {
    keypad.update();
    enc.handle();
}