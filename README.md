# ESP32 BLE HID Macropad

A compact, programmable macropad with Bluetooth connectivity, featuring 6 mechanical keys and a rotary encoder for media control.

![Version](https://img.shields.io/badge/Version-1.0-blue.svg)
![ESP32](https://img.shields.io/badge/Platform-ESP32-green.svg)
![BLE](https://img.shields.io/badge/Connectivity-BLE_HID-orange.svg)

## Features

- 6-key mechanical switch matrix
- Rotary encoder for volume/scroll control
- Bluetooth HID (no drivers required)
- Supports both keyboard and media controls
- Customizable key mappings

## Hardware

| Component | Quantity |
|-----------|----------|
| ESP32 Dev Board | 1 |
| Mechanical Keyswitches | 6 |
| Rotary Encoder | 1 |
| 1N4148 Diodes | 6 |

## Pinout

| Function | ESP32 Pin |
|----------|-----------|
| Row 1 | GPIO 5 |
| Row 2 | GPIO 6 |
| Col 1 | GPIO 4 |
| Col 2 | GPIO 7 |
| Col 3 | GPIO 8 |
| Encoder A | GPIO 9 |
| Encoder B | GPIO 10 |

## Installation

1. Wire components according to pinout table
2. Install Arduino IDE with ESP32 support
3. Install required libraries:
   - ESP32 BLE Arduino
   - RotaryEncoder
4. Upload firmware to ESP32

## Usage

1. Power on the macropad
2. Pair with "ESP32 Macropad" via Bluetooth
3. Default layout:
   - Top row: Media controls
   - Bottom row: Programmable macros
   - Encoder: Volume control

## Customization

Edit the keymap in the source code:

```cpp
const uint8_t keys[ROWS][COLS] = {
  {MEDIA_PLAY, MEDIA_VOL_UP, MEDIA_VOL_DOWN},
  {KEY_F13, KEY_F14, KEY_F15}
};
