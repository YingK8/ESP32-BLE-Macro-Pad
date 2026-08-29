"""Uplink parsing. The literals here are exactly what HostLink.cpp serialises,
so this file is the contract between the two sides."""

from __future__ import annotations

import pytest

from macropad_host.link import _parse_event


def test_key_down_and_up() -> None:
    down = _parse_event('{"k":2,"d":1}')
    assert (down.kind, down.key, down.down) == ("key", 2, True)
    up = _parse_event('{"k":2,"d":0}')
    assert (up.kind, up.key, up.down) == ("key", 2, False)


def test_encoder_delta_keeps_its_sign() -> None:
    event = _parse_event('{"e":-3}')
    assert (event.kind, event.delta) == ("encoder", -3)


@pytest.mark.parametrize(
    "line",
    [
        "",
        "ets Jul 29 2019 12:21:46",  # ESP32 ROM boot log
        '{"k":2,',  # truncated write
        "[1,2,3]",  # valid JSON, wrong shape
        '{"z":1}',  # unknown key
    ],
)
def test_noise_is_ignored_rather_than_raising(line: str) -> None:
    """The reader thread must survive boot logs and half-lines after a reconnect."""
    assert _parse_event(line) is None
