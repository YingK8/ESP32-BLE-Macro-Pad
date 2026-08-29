"""Ties the link, the scheduler and the active app together.

Deliberately threads rather than asyncio: rumps needs the main thread for the
macOS run loop, and mixing that with an event loop buys nothing here. There are
exactly two background threads -- APScheduler's worker and the serial reader --
and one lock guarding the active app.
"""

from __future__ import annotations

import logging
import threading
from collections.abc import Callable

import pluggy
from apscheduler.schedulers.background import BackgroundScheduler

from . import apps as builtin_apps
from .api import (
    HOOK_NAMESPACE,
    KEY_NEXT,
    KEY_PREV,
    App,
    Frame,
    InputEvent,
    MacroPadSpec,
)
from .config import Settings

log = logging.getLogger(__name__)

RENDER_JOB = "render"
DATA_JOB = "data"


def build_plugin_manager() -> pluggy.PluginManager:
    pm = pluggy.PluginManager(HOOK_NAMESPACE)
    pm.add_hookspecs(MacroPadSpec)
    pm.register(builtin_apps)
    # Anything installed declaring a "macropad" entry point is picked up too, so a
    # third-party app is a pip install away.
    pm.load_setuptools_entrypoints(HOOK_NAMESPACE)
    return pm


class Runtime:
    def __init__(
        self,
        settings: Settings,
        link,  # noqa: ANN001 - SerialLink or NullLink
        on_frame: Callable[[Frame], None] | None = None,
        on_state: Callable[[], None] | None = None,
        initial_app: str | None = None,
    ) -> None:
        self._settings = settings
        self._link = link
        self._on_frame = on_frame  # used by --dry-run to print instead of send
        self._on_state = on_state  # lets the menubar refresh its title
        self._lock = threading.RLock()
        self._scheduler = BackgroundScheduler(daemon=True)
        self._index = 0
        self._was_connected = False

        pm = build_plugin_manager()
        discovered: list[App] = [a for group in pm.hook.macropad_apps(config=settings) for a in group]
        by_name = {app.name: app for app in discovered}
        # config order is display order; unknown names are reported, not fatal
        self._apps = [by_name[n] for n in settings.apps if n in by_name]
        for missing in [n for n in settings.apps if n not in by_name]:
            log.warning("configured app %r not found; known: %s", missing, sorted(by_name))
        if not self._apps:
            self._apps = discovered

        # Resolved here but not activated: scheduling jobs before start() would
        # queue them tentatively and make the first app render twice.
        names = [app.name for app in self._apps]
        if initial_app and initial_app in names:
            self._index = names.index(initial_app)
        elif initial_app:
            log.warning("unknown --app %r; starting on %s", initial_app, names[0])

    # -- state -------------------------------------------------------------

    @property
    def apps(self) -> list[App]:
        return list(self._apps)

    @property
    def active(self) -> App | None:
        with self._lock:
            return self._apps[self._index] if self._apps else None

    @property
    def connected(self) -> bool:
        return bool(self._link.connected)

    @property
    def port_name(self) -> str | None:
        return self._link.port_name

    def select(self, index: int) -> None:
        with self._lock:
            if not self._apps:
                return
            self._index = index % len(self._apps)
            app = self._apps[self._index]
        log.info("active app: %s", app.name)
        self._schedule_for(app)
        self._tick()
        if self._on_state:
            self._on_state()

    def select_by_name(self, name: str) -> None:
        for i, app in enumerate(self._apps):
            if app.name == name:
                self.select(i)
                return

    def cycle(self, step: int) -> None:
        self.select(self._index + step)

    # -- lifecycle ---------------------------------------------------------

    def start(self) -> None:
        if not self._apps:
            raise RuntimeError("no apps enabled -- check the `apps` list in config.toml")
        self._scheduler.start()
        self._link.start()
        self.select(self._index)

    def stop(self) -> None:
        self._scheduler.shutdown(wait=False)
        self._link.stop()

    def handle_event(self, event: InputEvent) -> None:
        """Called from the serial reader thread."""
        if event.kind == "key" and event.down and event.key in (KEY_PREV, KEY_NEXT):
            self.cycle(-1 if event.key == KEY_PREV else 1)
            return
        # Hold the lock across on_input so it cannot interleave with render() --
        # the reader thread and the scheduler thread both reach into app state.
        with self._lock:
            app = self._apps[self._index] if self._apps else None
            if app is None:
                return
            try:
                app.on_input(event)
            except Exception:  # noqa: BLE001 - one bad app must not take the runtime down
                log.exception("%s.on_input failed", app.name)
        self._tick()  # redraw immediately rather than waiting for the next interval

    # -- internals ---------------------------------------------------------

    def _schedule_for(self, app: App) -> None:
        """Point the render/data jobs at the newly active app.

        Only the active app is polled: agenda's network fetch and nowplaying's
        AppleScript calls are not worth running for a screen nobody is looking at.
        """
        self._scheduler.add_job(
            self._tick,
            "interval",
            seconds=max(0.2, app.refresh_seconds),
            id=RENDER_JOB,
            replace_existing=True,
            max_instances=1,
            coalesce=True,
        )
        interval = getattr(app, "data_interval_seconds", None)
        if interval:
            self._scheduler.add_job(
                self._refresh_data,
                "interval",
                seconds=interval,
                id=DATA_JOB,
                replace_existing=True,
                max_instances=1,
                coalesce=True,
            )
        elif self._scheduler.get_job(DATA_JOB):
            self._scheduler.remove_job(DATA_JOB)
        self._scheduler.add_job(self._refresh_data, "date")  # one-shot, so the switch is not stale

    def _refresh_data(self) -> None:
        with self._lock:
            app = self._apps[self._index] if self._apps else None
        if app is None:
            return
        try:
            app.refresh_data()
        except Exception:  # noqa: BLE001
            log.exception("%s.refresh_data failed", app.name)

    def _tick(self) -> None:
        with self._lock:
            app = self._apps[self._index] if self._apps else None
            if app is None:
                return
            try:
                frame = app.render()
            except Exception:  # noqa: BLE001
                log.exception("%s.render failed", app.name)
                frame = Frame().add("RENDER ERROR", size=2).add(app.name, size=3)

        if self._on_frame:
            self._on_frame(frame)
        self._link.send(frame)

        connected = self.connected
        if connected != self._was_connected:
            self._was_connected = connected
            log.info("link %s", "up" if connected else "down")
            if self._on_state:
                self._on_state()
