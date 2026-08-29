#ifndef HOST_LINK_H
#define HOST_LINK_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Newline-delimited JSON over the C3's native USB CDC.
//
// Serial rather than BLE because the pad is already a BLE HID peripheral bonded
// to the host; a second central connecting to the same peripheral is unreliable.
// Keeping the two links apart means the screen never fights the macros.
//
//   down:  {"e":1,"l":[[2,"WORK 1/4"],[8,"24:13"]]}
//   up:    {"k":2,"d":1}   key 2 pressed
//          {"e":-1}        encoder moved one detent
class HostLink {
public:
    void begin();

    // Parse one frame into `doc`. Returns true only on a complete, valid frame.
    bool poll(JsonDocument& doc);

    void sendKey(uint8_t id, bool down);
    void sendEncoder(int delta);

    // True once a frame has arrived and the host has not gone quiet since.
    bool hostPresent() const;

private:
    void discardLine();

    uint32_t _lastFrameMs = 0;
    bool     _everSeen    = false;

    static constexpr uint32_t HOST_TIMEOUT_MS = 5000;
};

#endif  // HOST_LINK_H
