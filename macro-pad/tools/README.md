# Task Push Tools

Two Python scripts for sending a task list to the MacroPad over BLE. Both do the same job; `send_tasks.py` is simpler for quick CLI use, `push_tasks.py` has more flags (`--address`, `--timeout`).

## Requirements

```
pip install bleak
```

## Usage

```bash
# Inline tasks
python send_tasks.py "grasp map" "orca handoff" "teleop"

# From a file (one task per line)
python send_tasks.py --file tasks.txt

# push_tasks.py with optional flags
python push_tasks.py --name "ESP32 MacroPad" --timeout 8.0 "task one" "task two"
python push_tasks.py --address AA:BB:CC:DD:EE:FF --file tasks.txt
```

Tasks are automatically uppercased before sending — the display font only contains capital letters.

---

## How it works: end-to-end

### 1. BLE GATT — the transport layer

BLE (Bluetooth Low Energy) uses a client/server model called **GATT** (Generic Attribute Profile).

- The **ESP32 is the server**: it hosts a *Service* (a logical grouping) that contains a *Characteristic* (a named data slot). Think of a characteristic like a file you can read or write over the air.
- The **laptop is the client**: it connects, discovers the service, and writes bytes to the characteristic.

Services and characteristics are identified by **UUIDs** (128-bit IDs). Both tools and the firmware share the same pair:

```
Service UUID:        c3a7b7a0-3c1b-4d46-9f5c-9f0d9d1a9d01
Characteristic UUID: c3a7b7a0-3c1b-4d46-9f5c-9f0d9d1a9d02
```

### 2. MTU and chunking

BLE ATT (Attribute Protocol) has a default **MTU of 23 bytes**, leaving only **20 bytes of usable payload** per write (3 bytes are consumed by the ATT header). Without explicit MTU negotiation, any write larger than 20 bytes will be silently truncated or rejected.

The tools split the payload into 20-byte chunks and write them one at a time:

```python
for i in range(0, len(data), CHUNK_SIZE):
    await client.write_gatt_char(CHAR_UUID, data[i:i + CHUNK_SIZE], response=False)
```

`response=False` uses **Write Without Response** (BLE opcode 0x52), which is faster than a confirmed write but provides no acknowledgement. It is appropriate here because the terminator-detection scheme makes retries unnecessary.

### 3. The terminator protocol

Because the payload arrives as many small chunks, the ESP32 can't know when the transfer is complete from any single write. The protocol uses a sentinel string `--END--` to mark the end:

```
GRASP MAP\nORCA HANDOFF\nTELEOP\n--END--
│                                │
└─ newline-separated task names  └─ sentinel
```

The firmware accumulates chunks in a fixed buffer and scans for `--END--` after each write:

```cpp
// TaskSync.cpp — onWrite()
const char* term = strstr(_buf, "--END--");
if (term) {
    enqueue(_buf, term - _buf);  // copy everything before "--END--"
    _buf[0] = '\0';              // reset for next transfer
}
```

Everything before the sentinel is the complete payload; the sentinel itself is discarded.

### 4. Thread-safety with a spinlock

BLE callbacks run in a FreeRTOS task managed by the ESP32 BLE stack — a different task from the Arduino `loop()`. Sharing memory between them without protection causes **race conditions** (one task reads while the other writes, getting garbled data).

TaskSync uses a **spinlock** (`portMUX_TYPE`) to make the handoff atomic:

```cpp
// Writer side (BLE callback task):
portENTER_CRITICAL(&taskMux);
memcpy(pendingPayload, data, n);
payloadReady = true;
portEXIT_CRITICAL(&taskMux);

// Reader side (loop() task):
portENTER_CRITICAL(&taskMux);
strncpy(out, pendingPayload, maxLen - 1);
payloadReady = false;
portEXIT_CRITICAL(&taskMux);
```

`portENTER_CRITICAL` on a single-core ESP32-C3 disables interrupts for the critical section. `payloadReady` is marked `volatile` so the compiler doesn't cache its value in a register across the critical section boundary.

### 5. Parsing the payload

Once `loop()` picks up the payload via `getNextTaskPayload()`, `applyTasks()` parses it line by line:

```cpp
const char* nl = strchr(p, '\n');
size_t len = nl ? (size_t)(nl - p) : strlen(p);  // handles last line with no \n
```

Each task is uppercased (ASCII arithmetic: `*q -= 32` maps 'a'→'A'), stored in `tasks[][]`, and the display is flagged dirty for the next `loop()` iteration.

### 6. Display update

`buildScreen()` in `ui.cpp` rebuilds the entire screen as one flat string and hands it to LVGL's label renderer. Tasks appear as 12-character rows:

```
1 GRASP MAP  
2 ORCA HANDOF   ← truncated at 12 chars silently
3 TELEOP     <  ← selected row gets '<' cursor
```

---

## Full data flow

```
Python                ESP32 BLE stack          Arduino loop()        LVGL
──────                ───────────────          ──────────────        ────
tasks.upper()
→ payload string
→ split into 20B chunks
→ write_gatt_char() ×N
                      onWrite() ×N
                      accumulate in _buf
                      strstr("--END--") found
                      enqueue(payload)         getNextTaskPayload()
                                               applyTasks()
                                               tasksDirty = true
                                               updateTaskQueue()     buildScreen()
                                                                     lv_label_set_text()
                                                                     → pixels on display
```
