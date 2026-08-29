"""Built-in apps, registered with pluggy.

To add an app: write a BaseApp subclass in this package and construct it in
`macropad_apps` below. Nothing in the firmware changes -- that is the whole point
of keeping app logic on the host.
"""

from __future__ import annotations

from ..api import App, hookimpl
from .agenda import AgendaApp
from .nowplaying import NowPlayingApp
from .pomodoro import PomodoroApp
from .sysstats import SysStatsApp


@hookimpl
def macropad_apps(config) -> list[App]:  # noqa: ANN001 - config.Settings
    return [
        PomodoroApp(config.pomodoro),
        AgendaApp(config.agenda, config.canvas_token),
        NowPlayingApp(),
        SysStatsApp(),
    ]
