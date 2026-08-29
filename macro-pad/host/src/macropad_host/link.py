"""USB CDC transport to the pad.

Serial rather than BLE on purpose: the pad is already a BLE HID peripheral bonded
to this Mac, and a second central connecting to the same peripheral is exactly the
race that made the old BLE task-push unreliable. Splitting the two links means the
screen never contends with the macros.
"""

from __future__ import annotations

import json
import logging
import threading
import time
from collections.abc import Callable

import serial
from serial.tools import list_ports

from .api import InputEvent
from .frame import Frame

log = logging.getLogger(__name__)

ESPRESSIF_VID = 0x303A  # Espressif's USB vendor ID; the C3's native USB CDC enumerates under it
RECONNECT_DELAY_S = 2.0
READ_TIMEOUT_S = 0.2


def find_port() -> str | None:
    """First serial port that looks like an Espressif native-USB device."""
    for port in list_ports.comports():
        if port.vid == ESPRESSIF_VID:
            return port.device
    return None


def _parse_event(line: str) -> InputEvent | None:
    """Decode one uplink NDJSON line. Returns None for anything unrecognised."""
    try:
        msg = json.loads(line)
    except (json.JSONDecodeError, UnicodeDecodeError):
        return None  # boot log, debug print, or a half-line after a reconnect
    if not isinstance(msg, dict):
        return None
    if "k" in msg:
        return InputEvent(kind="key", key=int(msg["k"]), down=bool(msg.get("d", 0)))
    if "e" in msg:
        return InputEvent(kind="encoder", delta=int(msg["e"]))
    return None


class SerialLink:
    """Owns the port. One background thread reads; any thread may send."""

    def __init__(
        self,
        port: str = "auto",
        baudrate: int = 115200,
        on_event: Callable[[InputEvent], None] | None = None,
    ) -> None:
        self._configured_port = port
        self._baudrate = baudrate
        self._on_event = on_event
        self._serial: serial.Serial | None = None
        self._write_lock = threading.Lock()
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None

    def set_event_handler(self, handler: Callable[[InputEvent], None]) -> None:
        """Set after construction, since the runtime needs the link to exist first."""
        self._on_event = handler

    @property
    def connected(self) -> bool:
        return self._serial is not None and self._serial.is_open

    @property
    def port_name(self) -> str | None:
        return self._serial.port if self._serial else None

    def start(self) -> None:
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, name="macropad-serial", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=2.0)
        self._close()

    def send(self, frame: Frame) -> bool:
        """Write one frame. Returns False if the link is down (caller can ignore)."""
        if not self.connected:
            return False
        payload = frame.to_wire()
        try:
            with self._write_lock:
                self._serial.write(payload)  # type: ignore[union-attr]
            return True
        except serial.SerialException:
            log.warning("write failed; dropping link")
            self._close()
            return False

    def _open(self) -> bool:
        port = find_port() if self._configured_port == "auto" else self._configured_port
        if not port:
            return False
        try:
            # Build unopened so DTR/RTS can be cleared first: on the C3's USB
            # Serial/JTAG peripheral those lines drive the download-mode reset,
            # and pyserial asserts DTR on open, which would reboot the pad.
            ser = serial.Serial()
            ser.port = port
            ser.baudrate = self._baudrate  # ignored by native CDC, but pyserial wants it
            ser.dtr = False
            ser.rts = False
            ser.timeout = READ_TIMEOUT_S
            ser.open()
            ser.reset_input_buffer()
            self._serial = ser
            log.info("connected to %s", port)
            return True
        except (serial.SerialException, OSError) as exc:
            log.debug("open %s failed: %s", port, exc)
            return False

    def _close(self) -> None:
        if self._serial:
            try:
                self._serial.close()
            except Exception:  # noqa: BLE001 - closing a dead port must never raise
                pass
        self._serial = None

    def _run(self) -> None:
        while not self._stop.is_set():
            if not self.connected:
                if not self._open():
                    self._stop.wait(RECONNECT_DELAY_S)
                    continue
            try:
                raw = self._serial.readline()  # type: ignore[union-attr]
            except (serial.SerialException, OSError):
                log.info("pad disconnected")
                self._close()
                continue
            if not raw:
                continue  # read timeout, normal
            event = _parse_event(raw.decode("utf-8", errors="replace").strip())
            if event and self._on_event:
                try:
                    self._on_event(event)
                except Exception:  # noqa: BLE001 - an app bug must not kill the reader
                    log.exception("input handler raised")


class NullLink:
    """Stand-in used by --dry-run so the runtime can start with no hardware."""

    connected = False
    port_name = None

    def start(self) -> None: ...
    def stop(self) -> None: ...
    def set_event_handler(self, handler: Callable[[InputEvent], None]) -> None: ...

    def send(self, frame: Frame) -> bool:  # noqa: ARG002
        return False
