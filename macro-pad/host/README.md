# macropad-host

Desktop companion for the ESP32 MacroPad. Every app runs here and pushes finished
text frames to the pad over USB serial, so adding an app costs no ESP32 flash and
no reflashing.

## Setup

```bash
cd macro-pad/host
uv sync --extra dev
uv run macropad run            # menubar app
```

First run writes a commented config to
`~/Library/Application Support/macropad-host/config.toml`.

## Commands

| Command | What it does |
|---|---|
| `uv run macropad run` | menubar app; `--no-menubar` to stay in the terminal |
| `uv run macropad run --dry-run` | no hardware; prints frames as ASCII |
| `uv run macropad preview --app pomodoro` | render one frame and exit |
| `uv run macropad tasks "…" "…"` | replace the task list (no args = list it) |
| `uv run macropad ports` | list serial ports; `*` marks the auto-detected one |
| `uv run pytest` | frame-layout and protocol tests |

`preview` is the fastest way to check layout — it needs no pad attached.

## Controls

Four keys on the pad report to the host (see [PROTOCOL.md](../PROTOCOL.md)):

| Key | Everywhere | pomodoro | agenda | nowplaying |
|---|---|---|---|---|
| PREV / NEXT | switch app | | | |
| ACTION_A | | start / pause | open selected item | play / pause |
| ACTION_B | | tick task off | | next track |
| encoder | | select task | scroll list | *stays on HID volume* |

An app declares whether it wants the encoder; when it does not, the encoder keeps
sending HID volume keys straight from the pad, with no host round trip.

## Apps

| App | Source | Needs |
|---|---|---|
| `pomodoro` | timer + task list | nothing; `focus_shortcut` optionally names a macOS Shortcut |
| `agenda` | `.ics` feeds + bCourses deadlines | `ics_urls` in config, `MACROPAD_CANVAS_TOKEN` in env or `host/.env` |
| `nowplaying` | current track | `brew install nowplaying-cli` (falls back to AppleScript for Music/Spotify) |
| `sysstats` | CPU / memory / battery | nothing |

The Canvas token is read from the environment only, never from `config.toml`.

## Writing an app

Subclass `BaseApp`, then construct it in `apps/__init__.py`:

```python
from ..api import BaseApp, Frame

class WeatherApp(BaseApp):
    name = "weather"
    refresh_seconds = 30.0
    data_interval_seconds = 600.0   # slow work runs on its own timer

    def refresh_data(self):         # network, subprocesses -- never in render()
        self._temp = fetch()

    def render(self) -> Frame:
        return Frame().add("BERKELEY", size=2).add(f"{self._temp}C", size=6)
```

`render()` runs on the display tick and must be instant; anything that can block
belongs in `refresh_data()`. Only the active app is polled.

`sysstats.py` is the shortest one to copy.
