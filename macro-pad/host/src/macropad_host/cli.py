"""Command line entry point: `macropad run | preview | tasks | ports`."""

from __future__ import annotations

import argparse
import logging
import sys
import threading
from pathlib import Path

from . import config as config_mod
from . import tasks as task_store
from .frame import Frame
from .link import NullLink, SerialLink, find_port
from .runtime import Runtime


def _setup_logging(verbose: bool) -> None:
    logging.basicConfig(
        level=logging.DEBUG if verbose else logging.INFO,
        format="%(asctime)s %(levelname)-7s %(name)s: %(message)s",
        datefmt="%H:%M:%S",
    )
    # APScheduler narrates every job add at INFO; only its problems are interesting.
    logging.getLogger("apscheduler").setLevel(logging.WARNING)


def _build_runtime(args: argparse.Namespace, on_frame=None) -> Runtime:  # noqa: ANN001
    settings = config_mod.load()
    if args.port:
        settings.link.port = args.port
    link = NullLink() if args.dry_run else SerialLink(settings.link.port, settings.link.baudrate)
    runtime = Runtime(settings, link, on_frame=on_frame, initial_app=args.app)
    link.set_event_handler(runtime.handle_event)  # wired after both objects exist
    return runtime


def cmd_ports(_: argparse.Namespace) -> int:
    from serial.tools import list_ports

    found = list(list_ports.comports())
    if not found:
        print("no serial ports found")
        return 1
    auto = find_port()
    for port in found:
        mark = "*" if port.device == auto else " "
        vid = f"{port.vid:04x}" if port.vid is not None else "----"
        print(f"{mark} {port.device:<28} vid={vid} {port.description}")
    print("\n* = would be chosen by port = \"auto\"" if auto else "\nno Espressif device found")
    return 0


def cmd_tasks(args: argparse.Namespace) -> int:
    if args.file:
        texts = [ln for ln in args.file.read_text().splitlines()]
    else:
        texts = args.text
    if not texts:
        for task in task_store.load():
            print(("[x] " if task["done"] else "[ ] ") + task["text"])
        return 0
    saved = task_store.replace(texts)
    print(f"wrote {len(saved)} task(s) to {task_store.TASKS_PATH}")
    return 0


def cmd_preview(args: argparse.Namespace) -> int:
    """Render one frame to stdout. The fastest way to check layout with no hardware."""
    args.dry_run = True
    runtime = _build_runtime(args)
    app = runtime.active
    if app is None:
        print("no apps enabled", file=sys.stderr)
        return 1
    app.refresh_data()
    frame: Frame = app.render()
    print(f"-- {app.name} --")
    print(frame.to_ascii())
    print(f"\nwire ({len(frame.to_wire())} bytes): {frame.to_wire().decode().rstrip()}")
    return 0


def cmd_run(args: argparse.Namespace) -> int:
    on_frame = (lambda f: print(f.to_ascii(), flush=True)) if args.dry_run else None
    runtime = _build_runtime(args, on_frame=on_frame)
    runtime.start()

    if args.no_menubar or args.dry_run:
        print("running headless; Ctrl-C to stop", file=sys.stderr)
        try:
            threading.Event().wait()
        except KeyboardInterrupt:
            pass
        finally:
            runtime.stop()
        return 0

    from . import menubar  # imported late: pulls in AppKit, macOS only

    menubar.run(runtime)
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="macropad", description=__doc__)
    parser.add_argument("-v", "--verbose", action="store_true")
    sub = parser.add_subparsers(dest="command", required=True)

    def add_common(p: argparse.ArgumentParser) -> None:
        p.add_argument("--port", help="serial port; overrides config (\"auto\" to detect)")
        p.add_argument("--app", help="app to start on")
        p.add_argument("--dry-run", action="store_true", help="no hardware; print frames as ASCII")

    run_p = sub.add_parser("run", help="drive the pad")
    add_common(run_p)
    run_p.add_argument("--no-menubar", action="store_true", help="stay in the terminal")
    run_p.set_defaults(func=cmd_run)

    prev_p = sub.add_parser("preview", help="render one frame as ASCII and exit")
    add_common(prev_p)
    prev_p.set_defaults(func=cmd_preview)

    tasks_p = sub.add_parser("tasks", help="replace the task list (no args lists it)")
    tasks_p.add_argument("text", nargs="*")
    tasks_p.add_argument("--file", type=Path, help="read one task per line")
    tasks_p.set_defaults(func=cmd_tasks)

    ports_p = sub.add_parser("ports", help="list serial ports")
    ports_p.set_defaults(func=cmd_ports)

    args = parser.parse_args(argv)
    _setup_logging(args.verbose)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
