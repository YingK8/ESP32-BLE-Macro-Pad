#!/usr/bin/env python3
import argparse
import asyncio
from typing import List, Optional

from bleak import BleakClient, BleakScanner

TASK_SERVICE_UUID = "c3a7b7a0-3c1b-4d46-9f5c-9f0d9d1a9d01"
TASK_CHAR_UUID = "c3a7b7a0-3c1b-4d46-9f5c-9f0d9d1a9d02"
TERMINATOR = "\n--END--"
CHUNK_SIZE = 20   # BLE ATT default payload limit; safe without MTU negotiation


async def find_device(name: str, timeout: float) -> Optional[str]:
    devices = await BleakScanner.discover(timeout=timeout)
    for device in devices:
        if device.name == name:
            return device.address
    return None


def load_tasks_from_file(path: str) -> List[str]:
    with open(path, "r", encoding="utf-8") as handle:
        return [line.strip() for line in handle if line.strip()]


def chunk_payload(payload: str) -> List[bytes]:
    data = payload.encode("utf-8")
    return [data[i : i + CHUNK_SIZE] for i in range(0, len(data), CHUNK_SIZE)]


async def send_tasks(address: str, tasks: List[str]) -> None:
    tasks = [t.upper() for t in tasks]  # font is uppercase-only
    payload = "\n".join(tasks) + TERMINATOR
    chunks = chunk_payload(payload)
    async with BleakClient(address) as client:
        if not client.is_connected:
            raise RuntimeError("Failed to connect to device.")
        for chunk in chunks:
            await client.write_gatt_char(TASK_CHAR_UUID, chunk, response=False)


async def main() -> None:
    parser = argparse.ArgumentParser(description="Push task titles to the macropad over BLE.")
    parser.add_argument("--name", default="ESP32 MacroPad", help="BLE device name")
    parser.add_argument("--address", help="BLE address (skip discovery)")
    parser.add_argument("--timeout", type=float, default=5.0, help="Discovery timeout seconds")
    parser.add_argument("--file", help="Path to newline-delimited task file")
    parser.add_argument("tasks", nargs="*", help="Task titles")
    args = parser.parse_args()

    tasks = []
    if args.file:
        tasks.extend(load_tasks_from_file(args.file))
    tasks.extend([task for task in args.tasks if task.strip()])
    if not tasks:
        raise SystemExit("No tasks provided.")

    address = args.address
    if not address:
        address = await find_device(args.name, args.timeout)
        if not address:
            raise SystemExit(f"Device '{args.name}' not found.")

    await send_tasks(address, tasks)
    print(f"Sent {len(tasks)} task(s) to {address}.")


if __name__ == "__main__":
    asyncio.run(main())
