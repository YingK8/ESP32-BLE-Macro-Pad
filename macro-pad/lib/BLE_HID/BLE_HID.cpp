#include "BLE_HID.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>

// BLE HID Objects
static BLEHIDDevice* hid;
static BLECharacteristic* keyboardInput;
static BLECharacteristic* mediaInput;
bool isConnected = false;

// Combined HID Report Descriptor for Keyboard and Media Keys
static const uint8_t hidReportDescriptor[] = {
  // Keyboard Collection
  0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
  0x09, 0x06,        // Usage (Keyboard)
  0xA1, 0x01,        // Collection (Application)
  0x85, 0x01,        //   Report ID (1)
  0x05, 0x07,        //   Usage Page (Key Codes)
  0x19, 0xE0,        //   Usage Minimum (0xE0)
  0x29, 0xE7,        //   Usage Maximum (0xE7)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x01,        //   Logical Maximum (1)
  0x75, 0x01,        //   Report Size (1)
  0x95, 0x08,        //   Report Count (8)
  0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
  0x95, 0x01,        //   Report Count (1)
  0x75, 0x08,        //   Report Size (8)
  0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
  0x95, 0x05,        //   Report Count (5)
  0x75, 0x01,        //   Report Size (1)
  0x05, 0x08,        //   Usage Page (LEDs)
  0x19, 0x01,        //   Usage Minimum (Num Lock)
  0x29, 0x05,        //   Usage Maximum (Kana)
  0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
  0x95, 0x01,        //   Report Count (1)
  0x75, 0x03,        //   Report Size (3)
  0x91, 0x01,        //   Output (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
  0x95, 0x06,        //   Report Count (6)
  0x75, 0x08,        //   Report Size (8)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x65,        //   Logical Maximum (101)
  0x05, 0x07,        //   Usage Page (Key Codes)
  0x19, 0x00,        //   Usage Minimum (0x00)
  0x29, 0x65,        //   Usage Maximum (0x65)
  0x81, 0x00,        //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
  0xC0,              // End Collection (Keyboard)
  
  // Media Keys Collection (Consumer Control)
  0x05, 0x0C,        // Usage Page (Consumer)
  0x09, 0x01,        // Usage (Consumer Control)
  0xA1, 0x01,        // Collection (Application)
  0x85, 0x02,        //   Report ID (2)
  0x15, 0x00,        //   Logical Minimum (0)
  0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
  0x75, 0x10,        //   Report Size (16)
  0x95, 0x01,        //   Report Count (1)
  0x19, 0x00,        //   Usage Minimum (0) - We now define a range of valid usages...
  0x2A, 0xFF, 0x03,  //   Usage Maximum (1023) - ...instead of a single unassigned usage.
  0x81, 0x00,        //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
  0xC0,              // End Collection (Consumer Control)
};



// Map characters to HID keycodes
uint8_t charToHidCode(char c) {
  switch (c) {
    case 'a': return 0x04;
    case 'b': return 0x05;
    case 'c': return 0x06;
    case 'd': return 0x07;
    case 'e': return 0x08;
    case 'f': return 0x09;
    default: return 0x00; // No key pressed
  }
}

// Map special codes to media keycodes
uint16_t specialCodeToMediaCode(uint8_t code) {
  switch (code) {
    case KEY_PLAY_PAUSE: return 0xCD; // Play/Pause
    case KEY_VOL_UP:     return 0xE9; // Volume Up
    case KEY_VOL_DOWN:   return 0xEA; // Volume Down
    default: return 0x00; // No media key
  }
}

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    isConnected = true;
    Serial.println("Device connected");
  }

  void onDisconnect(BLEServer* pServer) {
    isConnected = false;
    Serial.println("Device disconnected");
    BLEDevice::startAdvertising();
    Serial.println("Advertising restarted");
  }
};

void ble_hid_setup() {
  BLEDevice::init("ESP32 HID Keypad");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  hid = new BLEHIDDevice(pServer);
  hid->reportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));
  hid->manufacturer()->setValue("ESP32 Keypad");
  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);

  keyboardInput = hid->inputReport(1);   // Report ID 1 (Keyboard)
  mediaInput = hid->inputReport(2);      // Report ID 2 (Media Keys)

  hid->startServices();

  BLESecurity *pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance(HID_KEYBOARD);
  pAdvertising->addServiceUUID(hid->hidService()->getUUID());

  // Add manufacturer data to help with device recognition
  BLEAdvertisementData advertisementData;
  advertisementData.setCompleteServices(BLEUUID(hid->hidService()->getUUID()));
  advertisementData.setName("ESP32 HID Keypad");
  pAdvertising->setAdvertisementData(advertisementData);
  
  pAdvertising->start();

  Serial.println("Advertising started. Connect to 'ESP32 HID Keypad'");
}

bool ble_is_connected() {
  return isConnected;
}

void ble_send_key(char key, bool pressed) {
  if (!isConnected) {
    Serial.println("Not connected to any device");
    return;
  }

  uint8_t hidCode = charToHidCode(key);
  
  if (pressed) {
    // Key press report: modifier, reserved, key1, key2, key3, key4, key5, key6
    uint8_t report[] = {0x00, 0x00, hidCode, 0x00, 0x00, 0x00, 0x00, 0x00};
    keyboardInput->setValue(report, sizeof(report));
    keyboardInput->notify();
    Serial.print("Key pressed: ");
    Serial.println(key);
  } else {
    // Key release report: all zeros except modifier and reserved
    uint8_t report[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    keyboardInput->setValue(report, sizeof(report));
    keyboardInput->notify();
    Serial.print("Key released: ");
    Serial.println(key);
  }
}

void ble_send_media_key(uint16_t keyCode) {
  if (!isConnected) return;
  if (!isConnected) {
    Serial.println("Not connected to any device");
    return;
  }

  // Convert the 16-bit key code to bytes (little-endian)
  uint8_t report[2] = {static_cast<uint8_t>(keyCode & 0xFF), static_cast<uint8_t>((keyCode >> 8) & 0xFF)};
  mediaInput->setValue(report, sizeof(report));
  mediaInput->notify();
  
  Serial.print("Media key sent: 0x");
  Serial.println(keyCode, HEX);
  
  // Send a release report after a short delay
  delay(50);
  uint8_t release[2] = {0x00, 0x00};
  mediaInput->setValue(release, sizeof(release));
  mediaInput->notify();
}


