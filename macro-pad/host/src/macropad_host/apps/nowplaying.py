"""Current track, so you can see what's playing without raising a window.

macOS has no public now-playing API. `nowplaying-cli` (brew install nowplaying-cli)
reads the private MediaRemote framework, but Apple has been restricting it, so we
fall back to AppleScript against Music and Spotify, which keep working.
"""

from __future__ import annotations

import shutil
import subprocess

from ..api import KEY_ACTION_A, KEY_ACTION_B, BaseApp, Frame, InputEvent

SPOTIFY_TELL = 'tell application "Spotify"'
MUSIC_TELL = 'tell application "Music"'


def _run(args: list[str], timeout: float = 3.0) -> str:
    try:
        done = subprocess.run(args, capture_output=True, timeout=timeout, check=False)
    except (OSError, subprocess.SubprocessError):
        return ""
    return done.stdout.decode("utf-8", errors="replace").strip()


def _osascript(script: str) -> str:
    return _run(["osascript", "-e", script])


class NowPlayingApp(BaseApp):
    name = "nowplaying"
    refresh_seconds = 5.0
    data_interval_seconds = 3.0
    wants_encoder = False  # leave the encoder on HID volume, which is what you want here

    def __init__(self) -> None:
        self._title = ""
        self._artist = ""
        self._source = ""
        self._have_cli = shutil.which("nowplaying-cli") is not None

    def refresh_data(self) -> None:
        if self._have_cli:
            out = _run(["nowplaying-cli", "get", "title", "artist"])
            lines = [ln.strip() for ln in out.splitlines()]
            # nowplaying-cli prints the literal "null" per field when nothing is playing
            if len(lines) >= 2 and lines[0] and lines[0] != "null":
                self._title, self._artist, self._source = lines[0], lines[1], "system"
                return

        for app, tell in (("Spotify", SPOTIFY_TELL), ("Music", MUSIC_TELL)):
            state = _osascript(f'{tell} to if it is running then return player state as text')
            if state != "playing":
                continue
            self._title = _osascript(f"{tell} to return name of current track")
            self._artist = _osascript(f"{tell} to return artist of current track")
            self._source = app
            return

        self._title = self._artist = self._source = ""

    def render(self) -> Frame:
        frame = Frame()
        if not self._title:
            frame.add("NOW PLAYING", size=2)
            frame.add("PAUSED", size=3)
            if not self._have_cli:
                frame.add("", size=1)
                frame.add("brew install nowplaying-cli for", size=1)
                frame.add("system-wide detection", size=1)
            return frame
        frame.add(self._source.upper(), size=1)
        frame.add(self._title, size=3)
        frame.add("", size=1)
        frame.add(self._artist, size=2)
        return frame

    def on_input(self, event: InputEvent) -> None:
        if event.kind != "key" or not event.down:
            return
        # The pad is the HID device, so it cannot type media keys at itself --
        # the host drives its own transport controls instead.
        command = {KEY_ACTION_A: "togglePlayPause", KEY_ACTION_B: "next"}.get(event.key)
        if not command:
            return
        if self._have_cli:
            _run(["nowplaying-cli", command])
        elif self._source in ("Spotify", "Music"):
            tell = SPOTIFY_TELL if self._source == "Spotify" else MUSIC_TELL
            verb = "playpause" if command == "togglePlayPause" else "next track"
            _osascript(f"{tell} to {verb}")
        self.refresh_data()
