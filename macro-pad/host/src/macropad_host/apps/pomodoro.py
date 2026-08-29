"""Pomodoro timer with the task list underneath, plus optional macOS Focus."""

from __future__ import annotations

import logging
import subprocess
import time

from .. import tasks as task_store
from ..api import KEY_ACTION_A, KEY_ACTION_B, BaseApp, Frame, InputEvent

log = logging.getLogger(__name__)

WORK, BREAK, LONG_BREAK = "WORK", "BREAK", "LONG"


class PomodoroApp(BaseApp):
    name = "pomodoro"
    refresh_seconds = 1.0
    data_interval_seconds = 2.0
    wants_encoder = True

    def __init__(self, settings) -> None:  # noqa: ANN001 - config.PomodoroSettings
        self._cfg = settings
        self._phase = WORK
        self._completed = 0  # work sessions finished in this cycle
        self._running = False
        self._deadline = 0.0  # monotonic seconds; only meaningful while running
        self._remaining = settings.work_min * 60.0  # frozen countdown while paused
        self._selected = 0
        self._tasks: list[dict] = task_store.load()
        self._tasks_mtime = task_store.mtime()

    # -- timing ------------------------------------------------------------

    def _phase_seconds(self, phase: str) -> float:
        return {
            WORK: self._cfg.work_min,
            BREAK: self._cfg.break_min,
            LONG_BREAK: self._cfg.long_break_min,
        }[phase] * 60.0

    def _left(self) -> float:
        """Seconds remaining, clamped at zero."""
        if self._running:
            return max(0.0, self._deadline - time.monotonic())
        return max(0.0, self._remaining)

    def _advance(self) -> None:
        """Move to the next phase and keep running -- pomodoro cycles are automatic."""
        if self._phase == WORK:
            self._completed += 1
            due_long = self._completed % self._cfg.sessions_before_long == 0
            self._phase = LONG_BREAK if due_long else BREAK
        else:
            self._phase = WORK
        self._remaining = self._phase_seconds(self._phase)
        self._deadline = time.monotonic() + self._remaining
        self._set_focus(self._phase == WORK)

    def _toggle(self) -> None:
        if self._running:
            self._remaining = self._left()  # freeze the countdown where it is
            self._running = False
        else:
            self._deadline = time.monotonic() + self._left()
            self._running = True
        self._set_focus(self._running and self._phase == WORK)

    def _set_focus(self, on: bool) -> None:
        """Run the configured macOS Shortcut. Named shortcuts are the only stable
        way to drive Focus modes from a script; there is no public API."""
        name = self._cfg.focus_shortcut
        if not name:
            return
        try:
            subprocess.run(
                ["shortcuts", "run", name, "--input-path", "-"],
                input=b"on" if on else b"off",
                timeout=5,
                check=False,
                capture_output=True,
            )
        except (OSError, subprocess.SubprocessError) as exc:
            log.warning("focus shortcut %r failed: %s", name, exc)

    # -- task list ---------------------------------------------------------

    def refresh_data(self) -> None:
        """Reload tasks only when the file actually changed on disk."""
        current = task_store.mtime()
        if current != self._tasks_mtime:
            self._tasks_mtime = current
            self._tasks = task_store.load()

    def _pending(self) -> list[dict]:
        """Undone tasks -- the only ones drawn, so also the only ones selectable."""
        return [t for t in self._tasks if not t["done"]]

    # -- app surface -------------------------------------------------------

    def render(self) -> Frame:
        if self._running and self._left() <= 0:
            self._advance()

        left = int(self._left())
        label = self._phase if self._running else "PAUSED"
        cycle = f"{self._completed % self._cfg.sessions_before_long}/{self._cfg.sessions_before_long}"

        frame = Frame(wants_encoder=True)
        frame.add(f"{label} {cycle}", size=2)
        frame.add(f"{left // 60:02d}:{left % 60:02d}", size=8)

        pending = self._pending()
        if pending:
            self._selected %= len(pending)  # the list shrinks as tasks are ticked off
            frame.add("", size=1)
            for i, task in enumerate(pending):
                marker = ">" if i == self._selected else " "
                frame.add(f"{marker}{task['text']}", size=2, indent=" ")
        return frame

    def on_input(self, event: InputEvent) -> None:
        pending = self._pending()
        if event.kind == "encoder":
            if pending:
                self._selected = (self._selected + event.delta) % len(pending)
        elif event.kind == "key" and event.down:
            if event.key == KEY_ACTION_A:
                self._toggle()
            elif event.key == KEY_ACTION_B and pending:
                pending[self._selected % len(pending)]["done"] = True
                task_store.save(self._tasks)
                self._tasks_mtime = task_store.mtime()  # our own write, not an external edit
