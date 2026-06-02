---
name: project-gpio21-uart-conflict
description: GPIO21 on ESP32-C3 is UART0 TX — unusable as key matrix input
metadata:
  type: project
---

GPIO21 (COL_PINS[1], middle column) is UART0 TX on ESP32-C3-DevKitM-1. The UART peripheral holds it HIGH, preventing key presses from being detected. Any button wired to that column will silently fail.

**Why:** Arduino ESP32 framework maps Serial (UART0) TX to GPIO21 by default. Even if Serial.begin() is commented out, other framework init may claim it.

**How to apply:** Never assign a key matrix column to GPIO21. Current working layout:
- COL_PINS = {4, 21, 8} — col 1 (GPIO21) is dead for input; leave as Key::none()
- NEXT button moved to (row0, col2) = GPIO1 × GPIO8
- USB CDC build flags added (-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1) to free GPIO21 for Serial output via native USB instead
