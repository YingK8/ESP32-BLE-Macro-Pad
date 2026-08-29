"""CPU / memory / battery at a glance. Also the simplest app to copy when
writing a new one."""

from __future__ import annotations

import psutil

from ..api import BaseApp, Frame


def _bar(percent: float, width: int) -> str:
    """A crude progress bar -- the built-in font has no block glyphs, so use #."""
    filled = int(round(percent / 100 * width))
    return "#" * filled + "." * (width - filled)


class SysStatsApp(BaseApp):
    name = "sysstats"
    refresh_seconds = 2.0
    data_interval_seconds = 2.0

    def __init__(self) -> None:
        self._cpu = 0.0
        self._mem = 0.0
        self._battery: psutil._common.sbattery | None = None
        psutil.cpu_percent(interval=None)  # prime the counter; the first call always reads 0

    def refresh_data(self) -> None:
        # interval=None compares against the previous call rather than blocking.
        self._cpu = psutil.cpu_percent(interval=None)
        self._mem = psutil.virtual_memory().percent
        self._battery = psutil.sensors_battery()

    def render(self) -> Frame:
        frame = Frame()
        frame.add("SYSTEM", size=2)
        frame.add(f"CPU {self._cpu:5.1f}%", size=3)
        frame.add(_bar(self._cpu, 23), size=2)
        frame.add(f"MEM {self._mem:5.1f}%", size=3)
        frame.add(_bar(self._mem, 23), size=2)
        if self._battery is not None:
            plug = "CHG" if self._battery.power_plugged else "BAT"
            frame.add(f"{plug} {self._battery.percent:.0f}%", size=2)
        return frame
