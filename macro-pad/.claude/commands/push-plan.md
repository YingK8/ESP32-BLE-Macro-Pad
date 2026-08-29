Parse a plan or schedule file and push the extracted tasks to the ESP32 MacroPad.

Arguments: $ARGUMENTS  (path to a plan.md, schedule.md, or any text/markdown file)

---

## Step 1 — Read the file

Read the file at the path given in $ARGUMENTS.
If no path is given, look for any of these in the current directory (in order):
  plan.md, schedule.md, tasks.md, tasks.txt
If none exist, ask the user for a file path.

---

## Step 2 — Extract tasks

Scan the file for task-like lines. Apply these rules in order:

**Markdown task lists (highest priority)**
Match lines like:
  - [ ] task name
  - [x] task name   ← skip completed tasks (checked boxes)
  * [ ] task name
Extract the text after `[ ] `.

**Numbered or bulleted lists**
Match lines like:
  - task name
  * task name
  1. task name
  2) task name
Strip the leading marker and whitespace.

**Table rows (schedule format)**
Match lines like:
  | 09:00 | Task name | 25min |
  | WORK  | Task name |
Extract the cell that looks like a task name — typically the longest non-time, non-duration cell.
Skip header rows (contain "---") and rows where the task cell is empty or a label like "BREAK".

**Headers as tasks (lowest priority — only if nothing else found)**
Match lines like:
  ## Task name
  ### Task name
Extract the text after `#` markers. Skip top-level `#` (document title).

**Always skip:**
- Lines that are only whitespace, punctuation, or markdown syntax
- Lines containing only times, durations, or phase labels (WORK, REST, BREAK, LONG REST)
- Comment lines starting with `<!--` or `//`

---

## Step 3 — Validate and trim

After extraction:
1. Strip leading/trailing whitespace from each task name.
2. Remove inline markdown (`**bold**`, `_italic_`, `` `code` ``, `[link](url)`).
3. Drop empty strings.
4. Deduplicate while preserving order.

There is no length or count limit any more — the host wraps each task to the panel
width and stops at the bottom edge. Keep names short anyway so more fit on screen,
and order by priority since the overflow is simply not drawn.

---

## Step 4 — Confirm with the user

Print the extracted task list before sending:

```
Extracted N task(s) from <filename>:
  1. TASK ONE
  2. TASK TWO
  ...

Send to MacroPad? (yes/no)
```

Wait for confirmation. If the user says no, stop here.

---

## Step 5 — Send

Run from `macro-pad/host/`:

```bash
uv run macropad tasks "Task one" "Task two" ...
```

Pass each task as a separate quoted argument, or write them one per line to a temp
file and use `--file` if the list is long.

Report success or failure. On failure, suggest:
- `uv sync` in `macro-pad/host/` if the command is not found
- `uv run macropad preview --app pomodoro` to see how the list will render

---

## Constraints

- Tasks are stored in `~/Library/Application Support/macropad-host/tasks.json`.
  Neither the pad nor `macropad run` needs to be running to write them; a running
  host picks the change up within ~2 seconds.
- Mixed case is fine — the built-in font has lowercase.
- Order by priority: anything past the bottom of the panel is simply not drawn.
