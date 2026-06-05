#include "BLE_HID.h"
#include <BLESecurity.h>

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
  0x81, 0x02,        //   Input (Data,Var,Abs)
  0x95, 0x01,        //   Report Count (1)
  0x75, 0x08,        //   Report Size (8)
  0x81, 0x01,        //   Input (Const,Array,Abs)
  0x95, 0x05,        //   Report Count (5)
  0x75, 0x01,        //   Report Size (1)
  0x05, 0x08,        //   Usage Page (LEDs)
  0x19, 0x01,        //   Usage Minimum (Num Lock)
  0x29, 0x05,        //   Usage Maximum (Kana)
  0x91, 0x02,        //   Output (Data,Var,Abs)
  0x95, 0x01,        //   Report Count (1)
  0x75, 0x03,        //   Report Size (3)
  0x91, 0x01,        //   Output (Const,Array,Abs)
  0x95, 0x06,        //   Report Count (6)
  0x75, 0x08,        //   Report Size (8)
  0x15, 0x00,        //   Logical Minimum (0)
  0x25, 0x65,        //   Logical Maximum (101)
  0x05, 0x07,        //   Usage Page (Key Codes)
  0x19, 0x00,        //   Usage Minimum (0x00)
  0x29, 0x65,        //   Usage Maximum (0x65)
  0x81, 0x00,        //   Input (Data,Array,Abs)
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
  0x19, 0x00,        //   Usage Minimum (0)
  0x2A, 0xFF, 0x03,  //   Usage Maximum (1023)
  0x81, 0x00,        //   Input (Data,Array,Abs)
  0xC0,              // End Collection (Consumer Control)
};

static BLE_HID* _instance = nullptr;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*) override {
        _instance->setConnected(true);
        Serial.println("BLE connected");
    }
    void onDisconnect(BLEServer*) override {
        _instance->setConnected(false);
        Serial.println("BLE disconnected — restarting advertising");
        BLEDevice::startAdvertising();
    }
};

void BLE_HID::begin(const char* deviceName) {
    _instance = this;

    // Tracing prints — if the device boot-loops or hangs, the last line you see
    // on serial pinpoints which step crashed.
    Serial.println("[BLE] init");
    BLEDevice::init(deviceName);

    Serial.println("[BLE] security");
    BLESecurity* security = new BLESecurity();
    security->setAuthenticationMode(ESP_LE_AUTH_BOND);   // back to legacy "Just Works" bonding
    security->setCapability(ESP_IO_CAP_NONE);
    security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    security->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    Serial.println("[BLE] server");
    _server = BLEDevice::createServer();
    _server->setCallbacks(new ServerCallbacks());

    Serial.println("[BLE] hid device");
    _hid = new BLEHIDDevice(_server);
    _hid->manufacturer()->setValue("ESP32");
    _hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
    _hid->hidInfo(0x00, 0x01);
    _hid->reportMap((uint8_t*)hidReportDescriptor, sizeof(hidReportDescriptor));

    _keyboardInput = _hid->inputReport(1);
    _mediaInput    = _hid->inputReport(2);

    Serial.println("[BLE] start services");
    _hid->startServices();

    // setBatteryLevel() internally calls notify() on the battery characteristic.
    // If called BEFORE startServices(), the characteristic's CCCD isn't fully
    // initialized yet — notify() then dereferences a half-built object and the
    // CPU traps (load access fault on 0x4540002f, MCAUSE=5). The fix is to call
    // it AFTER startServices(). Safe to skip on macOS/iOS; mainly for Windows.
    // _hid->setBatteryLevel(100);

    Serial.println("[BLE] advertising");
    _advertising = _server->getAdvertising();
    _advertising->setAppearance(HID_KEYBOARD);
    _advertising->addServiceUUID(_hid->hidService()->getUUID());
    _advertising->setScanResponse(true);
    _advertising->start();
    Serial.printf("[BLE] advertising as \"%s\"\n", deviceName);
}

// USB HID keyboard page: 'a'=0x04 … 'z'=0x1D, '1'=0x1E … '9'=0x26, '0'=0x27
uint8_t BLE_HID::charToHidCode(char c) {
    if (c >= 'a' && c <= 'z') return 0x04 + (c - 'a');
    if (c >= '1' && c <= '9') return 0x1E + (c - '1');
    if (c == '0') return 0x27;
    if (c == ' ') return 0x2C;
    if (c == '\n') return 0x28;  // Enter
    if (c == '\t') return 0x2B;  // Tab
    if (c == '-') return 0x2D;
    if (c == '=') return 0x2E;
    if (c == '/') return 0x38;
    if (c == '.') return 0x37;
    if (c == ',') return 0x36;
    return 0x00;
}

void BLE_HID::sendKey(char key, bool pressed) {
    if (!_isConnected) return;
    // NOTE: ideally we'd gate on _isAuthenticated too, but some BLE stacks never call
    // onAuthenticationComplete on bonded reconnects — that would silently disable all
    // output. Trust _isConnected; in practice the host re-requests after a missed notify.
    uint8_t hidCode = charToHidCode(key);
    if (pressed) {
        uint8_t report[] = { 0x00, 0x00, hidCode, 0x00, 0x00, 0x00, 0x00, 0x00 };
        _keyboardInput->setValue(report, sizeof(report));
    } else {
        uint8_t report[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        _keyboardInput->setValue(report, sizeof(report));
    }
    _keyboardInput->notify();
}

// Non-blocking: send the press now, schedule the release for tick() to flush.
// If a new press comes in while a release is still pending (fast encoder spin),
// flush the old release first so the host always sees a clean press→release→press.
void BLE_HID::sendMediaKey(uint16_t keyCode) {
    if (!_isConnected) return;
    if (_mediaReleasePending) {
        uint8_t release[2] = { 0x00, 0x00 };
        _mediaInput->setValue(release, sizeof(release));
        _mediaInput->notify();
    }
    uint8_t report[2] = {
        static_cast<uint8_t>(keyCode & 0xFF),
        static_cast<uint8_t>((keyCode >> 8) & 0xFF)
    };
    _mediaInput->setValue(report, sizeof(report));
    _mediaInput->notify();
    _mediaReleasePending = true;
    _mediaPressedAtMs    = millis();
}

// Call from loop() each iteration. Sends the deferred media-key release once
// MEDIA_RELEASE_MS has elapsed since the press — gives the host enough dwell
// to register the keystroke without blocking the main loop.
void BLE_HID::tick() {
    if (!_mediaReleasePending) return;
    if (!_isConnected) { _mediaReleasePending = false; return; }  // peer gone; drop it
    if ((millis() - _mediaPressedAtMs) < MEDIA_RELEASE_MS) return;
    uint8_t release[2] = { 0x00, 0x00 };
    _mediaInput->setValue(release, sizeof(release));
    _mediaInput->notify();
    _mediaReleasePending = false;
}
