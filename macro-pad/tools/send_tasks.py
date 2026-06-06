#!/usr/bin/env python3
"""
Send a task list to the ESP32 MacroPad over BLE GATT.

Usage:
  python send_tasks.py                          # interactive terminal UI
  python send_tasks.py "task one" "task two"    # one-shot CLI
  python send_tasks.py --file tasks.txt         # one task per line

Requirements: pip install bleak
"""

import asyncio
import sys
from bleak import BleakScanner, BleakClient

DEVICE_NAME  = "ESP32 MacroPad"
SERVICE_UUID = "c3a7b7a0-3c1b-4d46-9f5c-9f0d9d1a9d01"
CHAR_UUID    = "c3a7b7a0-3c1b-4d46-9f5c-9f0d9d1a9d02"
CHUNK_SIZE   = 20   # BLE write MTU used by ESP32 Arduino stack

# Mirror the device limits (include/Config.h) so what you preview is what displays
MAX_TASKS    = 12
TASK_MAX_LEN = 31   # TASK_MAX_LEN(32) minus the null terminator

# ANSI helpers — degrade to plain text when piped (e.g. into a file or another tool)
TTY  = sys.stdout.isatty()
def dim(s):  return f"\033[2m{s}\033[0m"  if TTY else s
def bold(s): return f"\033[1m{s}\033[0m"  if TTY else s
def warn(s): return f"\033[33m{s}\033[0m" if TTY else s
def good(s): return f"\033[32m{s}\033[0m" if TTY else s


# ── compaction ────────────────────────────────────────────────────────────────

def compact(raw: str) -> str:
    """Normalize one task to device constraints: collapse whitespace, uppercase,
    truncate to TASK_MAX_LEN at a word boundary (mid-word cuts read badly)."""
    s = " ".join(raw.split()).upper()  # split() w/o args eats tabs/doubled spaces too
    if len(s) <= TASK_MAX_LEN:
        return s
    cut = s.rfind(" ", 0, TASK_MAX_LEN + 1)
    return s[:cut] if cut > 0 else s[:TASK_MAX_LEN]  # hard cut only if one giant word


def compact_all(raw_tasks: list[str]) -> tuple[list[str], list[str]]:
    """Returns (compacted tasks, warnings). Drops empties, caps at MAX_TASKS."""
    tasks, warnings = [], []
    for raw in raw_tasks:
        if not raw.strip():
            continue
        c = compact(raw)
        if len(" ".join(raw.split())) > TASK_MAX_LEN:
            warnings.append(f"truncated: {raw.strip()!r} -> {c!r}")
        tasks.append(c)
    if len(tasks) > MAX_TASKS:
        warnings.append(f"dropped {len(tasks) - MAX_TASKS} task(s) over the {MAX_TASKS}-task limit")
        tasks = tasks[:MAX_TASKS]
    return tasks, warnings


# ── BLE transport ─────────────────────────────────────────────────────────────

async def send(tasks: list[str]) -> None:
    """Scan (with retries) and write the payload. Raises SystemExit on failure."""
    print(f"Scanning for '{DEVICE_NAME}'...")
    device = None
    for attempt in range(3):  # retry: radio may be mid-toggle or pad mid-reconnect
        device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=5.0)
        if device:
            break
        print(dim(f"  attempt {attempt + 1}/3: not seen yet"))
    if not device:
        print(warn("Device not found — is the MacroPad on and advertising?"))
        print(dim("  If it's connected to this Mac as a keyboard, old firmware stops"))
        print(dim("  advertising; toggle Bluetooth off/on while this script scans."))
        sys.exit(1)

    # Protocol: newline-separated task names, terminated with "--END--"
    data = ("\n".join(tasks) + "\n--END--").encode("utf-8")

    print(f"Connecting to {device.address}...")
    for attempt in range(3):  # connect can race the host's own reconnect; retry
        try:
            async with BleakClient(device) as client:
                for i in range(0, len(data), CHUNK_SIZE):
                    await client.write_gatt_char(CHAR_UUID, data[i:i + CHUNK_SIZE], response=False)
            print(good(f"Sent {len(tasks)} task(s): {', '.join(tasks)}"))
            return
        except Exception as e:
            print(dim(f"  connect attempt {attempt + 1}/3: {e}"))
            await asyncio.sleep(2)
    print(warn("Could not connect."))
    sys.exit(1)


# ── interactive TUI ───────────────────────────────────────────────────────────

def read_tasks_interactive() -> list[str]:
    """Prompt for tasks: one per line or comma-separated; blank line finishes."""
    print(bold("MacroPad task sender"))
    print(dim(f"One task per line (commas split too). Blank line = done. "
              f"Max {MAX_TASKS} tasks x {TASK_MAX_LEN} chars."))
    raw: list[str] = []
    while True:
        try:
            line = input(f"{len(raw) + 1:>2}> ")
        except (EOFError, KeyboardInterrupt):
            print()
            return raw
        if not line.strip():
            return raw
        raw.extend(part for part in line.split(","))


def preview(tasks: list[str], warnings: list[str]) -> None:
    print()
    for i, t in enumerate(tasks, 1):
        print(f"  {i:>2}. {t:<{TASK_MAX_LEN}} {dim(f'{len(t)} ch')}")
    for w in warnings:
        print(warn(f"  ! {w}"))
    print()


def interactive() -> None:
    while True:
        raw = read_tasks_interactive()
        tasks, warnings = compact_all(raw)
        if not tasks:
            print(dim("Nothing to send. Bye."))
            return
        preview(tasks, warnings)
        try:
            answer = input("Send? [Y/n/e(dit)] ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            print()
            return
        if answer in ("", "y", "yes"):
            asyncio.run(send(tasks))
            return
        if answer == "e":
            continue  # re-enter the list from scratch
        print(dim("Cancelled."))
        return


# ── CLI entry ─────────────────────────────────────────────────────────────────

def main() -> None:
    args = sys.argv[1:]
    if not args:
        interactive()
        return

    if args[0] == "--file":
        if len(args) < 2:
            print("Error: --file requires a filename")
            sys.exit(1)
        with open(args[1]) as f:
            task_list = [line for line in f if line.strip()]
    else:
        task_list = args

    tasks, warnings = compact_all(task_list)
    for w in warnings:
        print(warn(f"! {w}"))
    if not tasks:
        print("Error: no tasks provided")
        sys.exit(1)
    asyncio.run(send(tasks))


if __name__ == "__main__":
    main()
