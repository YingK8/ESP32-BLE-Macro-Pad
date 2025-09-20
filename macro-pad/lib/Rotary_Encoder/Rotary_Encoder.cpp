#include <RotaryEncoder.h>
#include "BLE_HID.h"

#define ROT_A 9
#define ROT_B 21
#define RECENT_CLICKS_LEN 5

static RotaryEncoder encoder(ROT_A, ROT_B, RotaryEncoder::LatchMode::TWO03);
static int oldPos = 0;

void encoder_setup() {
  // Initialize encoder pins
  pinMode(ROT_A, INPUT_PULLUP);
  pinMode(ROT_B, INPUT_PULLUP);
  encoder.setPosition(0);
}

void handleEncoder() {
  encoder.tick();
  // click_index = (click_index + 1) % RECENT_CLICKS_LEN;
  int newPos = encoder.getPosition();
  // recent_clicks[click_index] = newPos;

  if (newPos != oldPos) {
    if (oldPos > newPos) {
      Serial.println("Encoder: Volume Up");
      ble_send_media_key(0xE9); // Volume Up
    } else {
      Serial.println("Encoder: Volume Down");
      ble_send_media_key(0xEA); // Volume Down
    }
    oldPos = newPos;
  }
}
