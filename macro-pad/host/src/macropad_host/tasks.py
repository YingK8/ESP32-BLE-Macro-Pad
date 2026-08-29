"""The shared task list, stored as JSON next to the config.

Kept in a file rather than in memory so the `macropad tasks` CLI (and therefore
the /send-tasks and /push-plan slash commands) can update a running host.
"""

from __future__ import annotations

import json
from pathlib import Path

from .config import CONFIG_DIR

TASKS_PATH = CONFIG_DIR / "tasks.json"


def load(path: Path = TASKS_PATH) -> list[dict]:
    """Return [{"text": str, "done": bool}, ...]; empty if unset or corrupt."""
    try:
        data = json.loads(path.read_text())
    except (OSError, json.JSONDecodeError):
        return []
    if not isinstance(data, list):
        return []
    return [
        {"text": str(item.get("text", "")), "done": bool(item.get("done", False))}
        for item in data
        if isinstance(item, dict)
    ]


def save(tasks: list[dict], path: Path = TASKS_PATH) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(tasks, indent=2))


def replace(texts: list[str], path: Path = TASKS_PATH) -> list[dict]:
    """Overwrite the list with fresh, undone tasks. Blank entries are dropped."""
    tasks = [{"text": " ".join(t.split()), "done": False} for t in texts if t.strip()]
    save(tasks, path)
    return tasks


def mtime(path: Path = TASKS_PATH) -> float:
    """Modification time, or 0.0 if the file does not exist yet."""
    try:
        return path.stat().st_mtime
    except OSError:
        return 0.0
