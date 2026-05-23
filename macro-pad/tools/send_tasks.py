#!/usr/bin/env python3
"""
Send a task list to the ESP32 MacroPad over BLE GATT.

Usage:
  python send_tasks.py "task one" "task two" "task three"
  python send_tasks.py --file tasks.txt   # one task per line

Requirements: pip install bleak
"""

import asyncio
import sys
from bleak import BleakScanner, BleakClient

DEVICE_NAME  = "ESP32 MacroPad"
SERVICE_UUID = "c3a7b7a0-3c1b-4d46-9f5c-9f0d9d1a9d01"
CHAR_UUID    = "c3a7b7a0-3c1b-4d46-9f5c-9f0d9d1a9d02"
CHUNK_SIZE   = 20   # BLE write MTU used by ESP32 Arduino stack


async def send(tasks: list[str]) -> None:
    print(f"Scanning for '{DEVICE_NAME}'...")
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
    if not device:
        print("Device not found — is the MacroPad on and advertising?")
        sys.exit(1)

    # Protocol: newline-separated task names, terminated with "--END--"
    payload = "\n".join(tasks) + "\n--END--"
    data    = payload.encode("utf-8")

    print(f"Connecting to {device.address}...")
    async with BleakClient(device) as client:
        for i in range(0, len(data), CHUNK_SIZE):
            await client.write_gatt_char(CHAR_UUID, data[i:i + CHUNK_SIZE], response=False)
        print(f"Sent {len(tasks)} task(s): {', '.join(tasks)}")


def main() -> None:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        sys.exit(1)

    if args[0] == "--file":
        if len(args) < 2:
            print("Error: --file requires a filename")
            sys.exit(1)
        with open(args[1]) as f:
            task_list = [line.strip() for line in f if line.strip()]
    else:
        task_list = args

    if not task_list:
        print("Error: no tasks provided")
        sys.exit(1)

    asyncio.run(send(task_list))


if __name__ == "__main__":
    main()
