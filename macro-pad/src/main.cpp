#include <Arduino.h>
#include "BLE_HID.h"
#include "Rotary_Encoder.h"
#include "Keypad.h"

void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for Serial to initialize

  keypad_setup();
  encoder_setup();
  ble_hid_setup();
}

void loop() {
  handleEncoder();
  handleKeypad();
  delay(10); // Small delay to prevent overwhelming the system
}