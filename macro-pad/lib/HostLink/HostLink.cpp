#include "HostLink.h"

void HostLink::begin() {
    // deserializeJson() blocks waiting for the rest of a partial frame. The
    // default Stream timeout is 1000 ms, which would stall the key scan for a
    // whole second every time a write is split across USB packets.
    Serial.setTimeout(20);
}

bool HostLink::poll(JsonDocument& doc) {
    if (!Serial.available()) return false;

    // ArduinoJson reads straight from the Stream and stops at the end of the
    // object, so there is no framing buffer to size or overflow.
    DeserializationError err = deserializeJson(doc, Serial);
    if (err) {
        // EmptyInput just means we saw the newline left over from the last frame.
        if (err != DeserializationError::EmptyInput) discardLine();
        return false;
    }
    _lastFrameMs = millis();
    _everSeen    = true;
    return true;
}

// Skip to the next newline so one malformed frame cannot desynchronise the rest.
void HostLink::discardLine() {
    uint32_t deadline = millis() + 50;
    while (millis() < deadline) {
        if (!Serial.available()) continue;
        if (Serial.read() == '\n') return;
    }
}

void HostLink::sendKey(uint8_t id, bool down) {
    JsonDocument doc;
    doc["k"] = id;
    doc["d"] = down ? 1 : 0;
    serializeJson(doc, Serial);
    Serial.write('\n');
}

void HostLink::sendEncoder(int delta) {
    JsonDocument doc;
    doc["e"] = delta;
    serializeJson(doc, Serial);
    Serial.write('\n');
}

bool HostLink::hostPresent() const {
    return _everSeen && (millis() - _lastFrameMs) < HOST_TIMEOUT_MS;
}
