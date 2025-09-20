#include <Arduino.h>
#include "BLE_HID.h"
#include "Rotary_Encoder.h"

// --- Keypad Configuration ---
const byte ROWS = 2;
const byte COLS = 3;

uint8_t keys[ROWS][COLS] = {
  {'a', 'b', KEY_PLAY_PAUSE},  // Use codes instead of characters for media keys
  {'d', 'e', 'f'}
};

// Update these pins to match your ESP32 board's wiring
byte rowPins[ROWS] = {5, 1};
byte colPins[COLS] = {4, 20, 8};

// --- Keypad Debouncing Variables ---
unsigned long lastDebounceTime[ROWS][COLS] = {0};
unsigned long debounceDelay = 50;  // 50ms debounce time
bool lastButtonState[ROWS][COLS] = {{false}};
bool buttonState[ROWS][COLS] = {{false}};


void handleKeypad() {
  for (int r = 0; r < ROWS; r++) {
    digitalWrite(rowPins[r], LOW); // Activate the current row
    delayMicroseconds(10); // Short delay for stability
    
    for (int c = 0; c < COLS; c++) {
      bool currentState = (digitalRead(colPins[c]) == LOW);
      
      // Check if the button state has changed
      if (currentState != lastButtonState[r][c]) {
        lastDebounceTime[r][c] = millis();
      }
      
      // Apply debouncing
      if ((millis() - lastDebounceTime[r][c]) > debounceDelay) {
        // If the button state has changed, update the state
        if (currentState != buttonState[r][c]) {
          buttonState[r][c] = currentState;
          
          if (buttonState[r][c]) {
            // Key pressed
            uint8_t key = keys[r][c];
            
            // Check if it's a special media key or a regular key
            if (key == KEY_PLAY_PAUSE || key == KEY_VOL_UP || key == KEY_VOL_DOWN) {
              uint16_t mediaCode = specialCodeToMediaCode(key);
              Serial.print("Media key pressed: ");
              Serial.println(key);
              ble_send_media_key(mediaCode);
            } else {
              Serial.print("Key pressed: ");
              Serial.println((char)key);
              ble_send_key((char)key, true);
            }
          } else {
            // Key released - only send release for regular keys
            uint8_t key = keys[r][c];
            if (key != KEY_PLAY_PAUSE && key != KEY_VOL_UP && key != KEY_VOL_DOWN) {
              Serial.print("Key released: ");
              Serial.println((char)key);
              ble_send_key((char)key, false);
            }
          }
        }
      }
      
      lastButtonState[r][c] = currentState;
    }
    
    digitalWrite(rowPins[r], HIGH); // Deactivate the row
    delay(1); // Small delay for stability
  }
}



void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for Serial to initialize
  Serial.println("Starting BLE HID Keypad");

  // Initialize keypad pins
  for (int r = 0; r < ROWS; r++) {
    pinMode(rowPins[r], OUTPUT);
    digitalWrite(rowPins[r], HIGH); // Start with rows inactive
  }
  
  for (int c = 0; c < COLS; c++) {
    pinMode(colPins[c], INPUT_PULLUP); // Use internal pull-up resistors
  }

  encoder_setup();
  ble_hid_setup();
}

void loop() {
  handleEncoder();
  handleKeypad();
  delay(10); // Small delay to prevent overwhelming the system
}