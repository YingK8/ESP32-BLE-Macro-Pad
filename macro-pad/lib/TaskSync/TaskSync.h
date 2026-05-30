#pragma once
#include <Arduino.h>
#include "BLE_HID.h"

// Registers a BLE GATT service that accepts chunked task payloads over BLE WRITE.
// Payloads are buffered until the "--END--" terminator is received.
void setupTaskService(BLE_HID& ble);

// Returns true if a complete payload is waiting; copies it into out[maxLen] and clears the queue.
bool getNextTaskPayload(char* out, size_t maxLen);
