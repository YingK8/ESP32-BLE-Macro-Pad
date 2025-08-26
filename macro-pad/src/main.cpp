#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include "RotaryEncoder.h"

// Define service UUID
#define SERVICE_UUID        "1812"  // Human Interface Device Service
#define CHARACTERISTIC_UUID "2A4D"  // Keyboard Report Characteristic

// Encoder pins
#define ROT_A 9
#define ROT_B 10
RotaryEncoder encoder(ROT_A, ROT_B, RotaryEncoder::LatchMode::TWO03);

// Button matrix definition
#define ROWS 2
#define COLS 3
const int rowPins[ROWS] = {5, 6};
const int colPins[COLS] = {4, 7, 8};

// Button states
bool buttonStates[ROWS][COLS] = {{false}};
bool prevButtonStates[ROWS][COLS] = {{false}};

// Key mapping - define your desired keys here
const char* keyMap[ROWS][COLS] = {
  {"copy", "paste", "cut"},
  {"mute", "volup", "voldown"}
};

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing BLE keyboard...");

  // Initialize row pins (output)
  for (int r = 0; r < ROWS; r++) {
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], HIGH); // Start with rows high
  }

  // Initialize column pins (input with pullup)
  for (int c = 0; c < COLS; c++) {
    pinMode(colPins[c], INPUT_PULLUP);
  }

  Serial.println("BLE keyboard ready for connection");
}

void loop() {
  delay(1); // Small delay for debouncing
  // Scan matrix
  for (int r = 0; r < ROWS; r++) {
    digitalWrite(rowPins[r], LOW); // Activate current row
    for (int c = 0; c < COLS; c++) {
      buttonStates[r][c] = (digitalRead(colPins[c]) == LOW); // Pressed when LOW
      // Detect state changes
      if ((buttonStates[r][c] != prevButtonStates[r][c]) && buttonStates[r][c]) {
        Serial.println(keyMap[r][c]);
      }
      prevButtonStates[r][c] = buttonStates[r][c];
      }
      // Deactivate row
      digitalWrite(rowPins[r], HIGH);
    }
  }
  
//   // Handle encoder rotation
//   encoder.tick();
//   int newPos = encoder.getPosition();
//   static int lastPos = 0;
  
//   if (newPos != lastPos) {
//     if (newPos > lastPos) {
//       sendKey(VOLUP); // Volume up
//       Serial.println("Encoder: Volume Up");
//     } else {
//       sendKey(VOLDOWN); // Volume down
//       Serial.println("Encoder: Volume Down");
//     }
//     lastPos = newPos;
//     delay(50); // Debounce
//   }
  
//   delay(10);
// }