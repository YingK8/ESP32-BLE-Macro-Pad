"""macOS menubar UI. Switching apps from the Mac, mirroring the pad's own keys."""

from __future__ import annotations

import subprocess

import rumps

from .config import CONFIG_PATH
from .runtime import Runtime

CONNECTED = "⌨"
DISCONNECTED = "⌨ ✕"


class MenuBar(rumps.App):
    """Polls the runtime on a timer rather than taking callbacks from background
    threads -- AppKit menus may only be touched from the main thread."""

    def __init__(self, runtime: Runtime) -> None:
        super().__init__(CONNECTED, quit_button=None)
        self._runtime = runtime
        self._app_items: dict[str, rumps.MenuItem] = {}

        for app in runtime.apps:
            item = rumps.MenuItem(app.name, callback=self._select)
            self._app_items[app.name] = item
            self.menu.add(item)
        self.menu.add(rumps.separator)
        self.menu.add(rumps.MenuItem("Edit config…", callback=self._open_config))
        self.menu.add(rumps.MenuItem("Quit", callback=self._quit))

        self._status = rumps.MenuItem("", callback=None)
        self.menu.insert_before(runtime.apps[0].name, self._status)
        self.menu.insert_before(runtime.apps[0].name, rumps.separator)

        rumps.Timer(self._sync, 1).start()

    def _select(self, sender: rumps.MenuItem) -> None:
        self._runtime.select_by_name(sender.title)

    def _open_config(self, _: rumps.MenuItem) -> None:
        subprocess.run(["open", "-t", str(CONFIG_PATH)], check=False)

    def _quit(self, _: rumps.MenuItem) -> None:
        self._runtime.stop()
        rumps.quit_application()

    def _sync(self, _: rumps.Timer) -> None:
        connected = self._runtime.connected
        self.title = CONNECTED if connected else DISCONNECTED
        port = self._runtime.port_name if connected else None
        self._status.title = f"Pad on {port}" if port else "Pad not connected"
        active = self._runtime.active
        for name, item in self._app_items.items():
            item.state = 1 if active is not None and active.name == name else 0


def run(runtime: Runtime) -> None:
    """Blocks on the AppKit run loop until Quit."""
    MenuBar(runtime).run()
