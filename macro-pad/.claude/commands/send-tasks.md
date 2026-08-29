Send a task list to the ESP32 MacroPad.

Arguments: $ARGUMENTS

Steps:
1. Parse $ARGUMENTS as a space- or newline-separated list of task names.
   - If it looks like a file path, pass it with --file instead.
   - If no arguments, run the command with no task arguments to print the current list.
2. Run from `macro-pad/host/`:
   ```
   uv run macropad tasks "<task1>" "<task2>" ...
   ```
   or for a file:
   ```
   uv run macropad tasks --file <filename>
   ```
3. Report whether the write succeeded, and how many tasks were stored.

Notes:
- Tasks are written to `~/Library/Application Support/macropad-host/tasks.json`.
  A running `macropad run` picks the change up within ~2 seconds; the pad does not
  need to be connected at the time.
- The pomodoro app renders the list under the timer. Encoder scrolls the selection,
  ACTION_B ticks the selected task off.
- No length or count limit: the host wraps text to the panel and drops whatever
  runs past the bottom, so put the important tasks first.
- Mixed case is fine — the built-in font has lowercase.
