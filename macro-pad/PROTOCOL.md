# Host ↔ pad protocol

Newline-delimited JSON over the ESP32-C3's native USB CDC serial port. One JSON
object per line, both directions.

## Why serial and not BLE

The pad is a BLE HID peripheral bonded to the Mac. A second central connecting to
the same peripheral to push screen data is unreliable — that is what made the old
`send_tasks.py` need a Bluetooth toggle and a scan-retry race. Keeping the screen
on USB means the two links never contend: **BLE carries macros, USB carries the
screen.** It also removes MTU negotiation and chunking entirely; a whole frame is
one write.

## Down: host → pad

```json
{"e":1,"l":[[2,"WORK 1/4"],[8,"24:13"],[1,""],[2,">Write lab report"]]}
```

| Field | Meaning |
|---|---|
| `e` | 1 = forward encoder deltas to the host; 0 = encoder emits HID volume keys |
| `l` | lines, top to bottom, each `[textSize, text]` |

`e` travels with the frame rather than as a separate mode command so the input
mode can never drift out of sync with what is on screen.

The pad does no layout: lines are stacked from y=0, each consuming `8 * size`
pixels. Text is already wrapped and clipped by the host.

## Up: pad → host

```json
{"k":2,"d":1}
{"e":-1}
```

| Message | Meaning |
|---|---|
| `{"k":<id>,"d":<0\|1>}` | host key `id` released / pressed |
| `{"e":<delta>}` | encoder moved `delta` detents |

Key ids are `AppKey::` in `lib/MacroPad/src/MacroPad.h` and `KEY_*` in
`host/src/macropad_host/api.py` — **these two must stay in sync**:

| id | Name | Handled by |
|---|---|---|
| 0 | `PREV` | host runtime — previous app |
| 1 | `NEXT` | host runtime — next app |
| 2 | `ACTION_A` | the active app |
| 3 | `ACTION_B` | the active app |

Keys mapped to `Key::combo(...)` never appear here; they go out over BLE HID as
before and the host never sees them.

## The character grid

Arduino_GFX's built-in font is a 5×7 glyph in a 6×8 px cell, scaled by an integer
`setTextSize(s)`. On the 280×240 landscape surface:

| size | columns | rows | use |
|---|---|---|---|
| 1 | 46 | 30 | hints, fine print |
| 2 | 23 | 15 | body text, lists |
| 3 | 15 | 10 | headline |
| 4 | 11 | 7 | |
| 8 | 5 | 3 | `MM:SS` — exactly 5 characters |

These constants live in two places and must agree: `CELL_W`/`CELL_H` and
`TFT_WIDTH`/`TFT_HEIGHT` in `include/Config.h` and `lib/Screen/Screen.h`, and
`PANEL_W`/`PANEL_H`/`CELL_W`/`CELL_H` in `host/src/macropad_host/frame.py`.
`tests/test_frame.py::test_grid_dimensions` pins the table above.

## Implementation notes

**Parsing costs no code.** ArduinoJson deserialises straight from a `Stream`, so
the firmware never buffers or scans for a frame delimiter:

```cpp
JsonDocument doc;
if (deserializeJson(doc, Serial) == DeserializationError::Ok) { ... }
```

The catch is that it *blocks* waiting for the rest of a partial frame, so
`HostLink::begin()` sets `Serial.setTimeout(20)`. At the 1000 ms default a write
split across USB packets would stall the key scan for a full second.

**No framebuffer.** A 280×240×16bpp canvas is 134 KB against the C3's 320 KB of
RAM. `Screen` instead caches the last frame as text and repaints only lines whose
content changed — except when the *size* vector changes, which shifts every line
below it and forces a full redraw.

**Resync.** A malformed frame makes `HostLink::poll()` skip to the next newline,
so one bad write cannot desynchronise the stream. On the host side
`link._parse_event` returns `None` for anything unrecognised, which is what keeps
the reader thread alive through the ESP32 boot log.

**Opening the port does not reset the pad.** pyserial asserts DTR on open, and on
the C3's USB Serial/JTAG peripheral DTR/RTS drive the download-mode reset.
`SerialLink._open()` builds the port unopened, clears both lines, then opens.
