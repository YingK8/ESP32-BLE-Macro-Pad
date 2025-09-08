#include <Arduino.h>
#include <Keypad.h>

#define LED_PIN 0

const byte ROWS = 2;
const byte COLS = 3;
char hexaKeys[ROWS][COLS] = {
  {'C', 'V', 'X'},
  {'M', 'U', 'D'}
};
byte rowPins[ROWS] = {6, 5};
byte colPins[COLS] = {4, 7, 8};

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  Serial.println("cow");
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);
  delay(100);

  if (keypad.getKeys()) {
    for (int i = 0; i < LIST_MAX; i++) {
      if (keypad.key[i].stateChanged && keypad.key[i].kstate == PRESSED) {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("Key pressed: ");
        Serial.println(keypad.key[i].kchar);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
      }
    }
  }

  delay(10); // Small delay to help BLE stack process events
}
