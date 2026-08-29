# ESP32 BLE HID Macropad

A compact, programmable macropad with Bluetooth connectivity, featuring 6 mechanical keys and a rotary encoder for media control.

![Version](https://img.shields.io/badge/Version-1.0-blue.svg)
![ESP32](https://img.shields.io/badge/Platform-ESP32-green.svg)
![BLE](https://img.shields.io/badge/Connectivity-BLE_HID-orange.svg)

## Features

- 6-key mechanical switch matrix
- Rotary encoder for volume/scroll control
- Bluetooth HID (no drivers required)
- ST7789 240×280 screen driven by a desktop companion app over USB
- Productivity apps — pomodoro, class/assignment agenda, now playing, system stats —
  that live on the PC, so adding one needs no reflashing

## Hardware

| Component | Quantity |
|-----------|----------|
| ESP32-C3 Dev Board | 1 |
| Mechanical Keyswitches | 6 |
| Rotary Encoder | 1 |
| 1N4148 Diodes | 6 |
| ST7789 240×280 SPI display | 1 |

## Pinout

Authoritative source is [`macro-pad/include/Config.h`](macro-pad/include/Config.h).

| Function | ESP32 Pin |
|----------|-----------|
| Row 1 | GPIO 1 |
| Row 2 | GPIO 5 |
| Col 1 | GPIO 4 |
| Col 2 | GPIO 21 |
| Col 3 | GPIO 8 |
| Encoder A | GPIO 3 |
| Encoder B | GPIO 10 |
| TFT SCK / MOSI | GPIO 7 / 6 |
| TFT DC / CS / RST | GPIO 2 / 0 / 9 |
| TFT backlight | GPIO 20 |

> GPIO21 is UART0 TX on the ESP32-C3. The `ARDUINO_USB_CDC_ON_BOOT` build flag
> routes `Serial` to native USB to free it; if the two column-1 keys do not
> register, that pin is the reason.

## Installation

1. Wire components according to the pinout table.
2. Flash the firmware (PlatformIO pulls the libraries itself):
   ```bash
   cd macro-pad && pio run -t upload
   ```
3. Set up the desktop app:
   ```bash
   cd macro-pad/host && uv sync && uv run macropad run
   ```

## Usage

1. Power on the macropad and pair with "ESP32 MacroPad" via Bluetooth — this
   carries the keyboard macros.
2. Plug it in over USB — this carries the screen. The menubar icon shows the link
   state and lets you switch apps.
3. Default layout:

   | | Col 1 (GPIO4) | Col 2 (GPIO21) | Col 3 (GPIO8) |
   |---|---|---|---|
   | **Row 1** | previous app | ⌘⇧4 screenshot | next app |
   | **Row 2** | action A | ⌘Space Spotlight | action B |

   The encoder controls volume, unless the active app asks for it.

## Architecture

The pad is a dumb character-cell terminal: the PC renders each app to text lines
and pushes finished frames down the wire, and the pad reports key and encoder
events back. App logic, API tokens and text never touch the ESP32, which is what
keeps the firmware at ~48% of the C3's 1.25 MB app slot no matter how many apps
exist.

- [`macro-pad/PROTOCOL.md`](macro-pad/PROTOCOL.md) — wire format, key ids, grid maths
- [`macro-pad/host/README.md`](macro-pad/host/README.md) — desktop app, apps, writing your own

## Customization

Macro keys are the `Key::combo(...)` entries in
[`macro-pad/src/main.cpp`](macro-pad/src/main.cpp); `Key::app(...)` entries are
forwarded to the host instead of being typed. Everything else — what each screen
shows, what the action keys do — is Python in `macro-pad/host/`.
