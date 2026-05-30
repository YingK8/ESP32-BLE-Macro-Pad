#include "TaskSync.h"
#include <BLEServer.h>
#include <BLEService.h>
#include <BLECharacteristic.h>
#include <BLEAdvertising.h>
#include <string.h>

// Max bytes for a full task payload (MAX_TASKS × TASK_MAX_LEN + newlines + terminator).
// Fixed-size buffers avoid heap allocation in BLE callbacks.
static constexpr size_t BUF_SIZE = 512;

static const char* TASK_SERVICE_UUID = "c3a7b7a0-3c1b-4d46-9f5c-9f0d9d1a9d01";
static const char* TASK_CHAR_UUID    = "c3a7b7a0-3c1b-4d46-9f5c-9f0d9d1a9d02";

static portMUX_TYPE  taskMux      = portMUX_INITIALIZER_UNLOCKED;
static char          pendingPayload[BUF_SIZE];
static volatile bool payloadReady = false;

static void enqueue(const char* data, size_t len) {
    portENTER_CRITICAL(&taskMux);
    size_t n = len < BUF_SIZE - 1 ? len : BUF_SIZE - 1;
    memcpy(pendingPayload, data, n);
    pendingPayload[n] = '\0';
    payloadReady = true;
    portEXIT_CRITICAL(&taskMux);
}

// Accumulates chunked BLE writes (typically 20 bytes each) until "--END--" is received.
class TaskCharacteristicCallbacks : public BLECharacteristicCallbacks {
public:
    void onWrite(BLECharacteristic* ch) override {
        String val = ch->getValue();  // BLE library forces String here; convert immediately
        if (val.isEmpty()) return;
        size_t vlen = val.length();
        size_t blen = strlen(_buf);
        if (blen + vlen < BUF_SIZE - 1) {
            memcpy(_buf + blen, val.c_str(), vlen);
            _buf[blen + vlen] = '\0';
        }
        const char* term = strstr(_buf, "--END--");
        if (term) {
            enqueue(_buf, term - _buf);
            _buf[0] = '\0';
        }
    }
private:
    char _buf[BUF_SIZE] = {};
};

void setupTaskService(BLE_HID& ble) {
    BLEServer* server = ble.server();
    if (!server) return;
    BLEService* service = server->createService(TASK_SERVICE_UUID);
    BLECharacteristic* ch = service->createCharacteristic(
        TASK_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    ch->setCallbacks(new TaskCharacteristicCallbacks());
    service->start();
    BLEAdvertising* adv = ble.advertising();
    if (adv) { adv->addServiceUUID(TASK_SERVICE_UUID); adv->start(); }
}

bool getNextTaskPayload(char* out, size_t maxLen) {
    if (!payloadReady) return false;
    portENTER_CRITICAL(&taskMux);
    strncpy(out, pendingPayload, maxLen - 1);
    out[maxLen - 1] = '\0';
    payloadReady = false;
    portEXIT_CRITICAL(&taskMux);
    return true;
}
