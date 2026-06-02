Send a task list to the ESP32 MacroPad over BLE.

Arguments: $ARGUMENTS

Steps:
1. Parse $ARGUMENTS as a space- or newline-separated list of task names.
   - If it looks like a file path, pass it with --file instead.
   - If no arguments, ask the user for a task list.
2. Run the sender script from the project root:
   ```
   python tools/send_tasks.py <task1> <task2> ...
   ```
   or for a file:
   ```
   python tools/send_tasks.py --file <filename>
   ```
3. Report whether the send succeeded or failed.

Notes:
- The MacroPad must be powered on and BLE advertising ("ESP32 MacroPad").
- Requires: `pip install bleak`
- Task names are uppercased automatically — the display font only has capital letters.
- Max 12 tasks, max 31 chars each (longer names are silently truncated on device).
- Service UUID:  c3a7b7a0-3c1b-4d46-9f5c-9f0d9d1a9d01
- Char UUID:     c3a7b7a0-3c1b-4d46-9f5c-9f0d9d1a9d02
