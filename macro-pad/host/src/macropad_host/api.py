"""The surface an app plugin sees. Import from here, not from internals."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Protocol, runtime_checkable

import pluggy

from .frame import Frame, Line  # re-exported for app authors

HOOK_NAMESPACE = "macropad"
hookspec = pluggy.HookspecMarker(HOOK_NAMESPACE)
hookimpl = pluggy.HookimplMarker(HOOK_NAMESPACE)

__all__ = [
    "Frame",
    "Line",
    "InputEvent",
    "App",
    "BaseApp",
    "hookspec",
    "hookimpl",
    "MacroPadSpec",
    "KEY_PREV",
    "KEY_NEXT",
    "KEY_ACTION_A",
    "KEY_ACTION_B",
]

# Host-key ids as reported by the firmware's Key::app(id) entries.
# PREV/NEXT are consumed by the runtime; the two ACTION keys reach the active app.
KEY_PREV = 0
KEY_NEXT = 1
KEY_ACTION_A = 2
KEY_ACTION_B = 3


@dataclass(frozen=True)
class InputEvent:
    """One button or encoder event from the device.

    Exactly one of `key` / `delta` is meaningful, selected by `kind`.
    """

    kind: str  # "key" | "encoder"
    key: int = -1
    down: bool = False
    delta: int = 0


@runtime_checkable
class App(Protocol):
    """A screen the pad can show. Instances are created once at startup."""

    name: str  # short label, also the key used in config
    refresh_seconds: float  # how often runtime re-renders while this app is active

    def render(self) -> Frame:
        """Return the current screen. Must not block for long -- it runs on the scheduler."""
        ...

    def on_input(self, event: InputEvent) -> None:
        """Handle an action-key press or encoder turn while this app is active."""
        ...


class BaseApp:
    """Convenience base with sane defaults. Subclass this to write an app.

    The split between `render` and `refresh_data` matters: `render` runs on the
    display tick and must be instant, so anything that touches the network or
    shells out belongs in `refresh_data`, which runs on its own slow timer and
    caches into instance state.
    """

    name: str = "app"
    refresh_seconds: float = 1.0
    data_interval_seconds: float | None = None  # None = never call refresh_data
    wants_encoder: bool = False

    def render(self) -> Frame:
        raise NotImplementedError

    def refresh_data(self) -> None:
        """Slow work: network calls, subprocesses. Runs off the render path."""

    def on_input(self, event: InputEvent) -> None:
        """Ignore input by default."""


class MacroPadSpec:
    """Hook specifications. A plugin module implements `macropad_apps`."""

    @hookspec
    def macropad_apps(self, config) -> list[App]:  # noqa: ANN001 - avoids circular import
        """Return the App instances this plugin provides."""
