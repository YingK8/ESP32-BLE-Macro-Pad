Parse a plan or schedule file and push the extracted tasks to the ESP32 MacroPad over BLE.

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
3. Truncate to 31 characters (device hard limit — longer names are silently cut on display).
4. Drop empty strings.
5. Deduplicate while preserving order.
6. Keep at most 12 tasks (device limit). If more are found, take the first 12 and warn the user.

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

Run from the project root:

```bash
python tools/send_tasks.py "TASK ONE" "TASK TWO" ...
```

Pass each task as a separate quoted argument.
Alternatively, write the tasks to a temp file and use --file if the list is long.

Report success or failure. On failure (device not found, BLE error), suggest:
- Check the MacroPad is powered on and advertising as "ESP32 MacroPad"
- Run `pip install bleak` if the import fails
- Try `python tools/push_tasks.py --timeout 10.0 ...` for a longer scan window

---

## Constraints

- MacroPad must be on and BLE-advertising before running.
- Requires: `pip install bleak`
- Tasks are uppercased on send — the display font only has capital letters.
- Max 12 tasks, max 31 chars each.
- The MacroPad shows tasks one screen-page at a time (5 visible rows).
  Recommend ordering tasks by priority so the most important appear first.
