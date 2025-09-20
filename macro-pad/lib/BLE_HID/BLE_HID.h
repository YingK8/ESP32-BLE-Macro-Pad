#ifndef BLE_HID_H
#define BLE_HID_H

#include <Arduino.h>

// Define keys - use special codes for media keys
#define KEY_PLAY_PAUSE 0x01
#define KEY_VOL_UP     0x02
#define KEY_VOL_DOWN   0x03

void ble_hid_setup();
bool ble_is_connected();
void ble_send_key(char key, bool pressed);
void ble_send_media_key(uint16_t keyCode);
uint16_t specialCodeToMediaCode(uint8_t code);

#endif // BLE_HID_H