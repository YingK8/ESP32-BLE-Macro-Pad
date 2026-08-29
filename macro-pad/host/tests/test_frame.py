"""Wrapping and clipping are the likeliest source of a garbled screen, so they
are the part worth pinning down with tests."""

from __future__ import annotations

import json

import pytest

from macropad_host.frame import PANEL_H, Frame, Line, cols_for, rows_for


@pytest.mark.parametrize(
    ("size", "cols", "rows"),
    [(1, 46, 30), (2, 23, 15), (3, 15, 10), (4, 11, 7), (8, 5, 3)],
)
def test_grid_dimensions(size: int, cols: int, rows: int) -> None:
    """These numbers must match Screen.cpp's cell maths on the firmware side."""
    assert cols_for(size) == cols
    assert rows_for(size) == rows


def test_wraps_at_the_size_column_count() -> None:
    frame = Frame([Line("a" * 30, size=2)])  # 23 columns at size 2
    assert [line.text for line in frame.fit()] == ["a" * 23, "a" * 7]


def test_long_word_is_broken_rather_than_overflowing() -> None:
    frame = Frame([Line("supercalifragilistic", size=3)])  # 15 columns
    fitted = frame.fit()
    assert all(len(line.text) <= 15 for line in fitted)
    assert "".join(line.text for line in fitted) == "supercalifragilistic"


def test_blank_line_survives_as_a_spacer() -> None:
    assert [line.text for line in Frame([Line("", size=1)]).fit()] == [""]


def test_clips_at_the_panel_bottom() -> None:
    # Size 8 lines are 64px tall, so only 3 fit in 240px.
    frame = Frame([Line(f"L{i}", size=8) for i in range(6)])
    fitted = frame.fit()
    assert len(fitted) == 3
    assert sum(line.size * 8 for line in fitted) <= PANEL_H


def test_overflow_stops_layout_rather_than_skipping_ahead() -> None:
    """A later small line must not jump the queue past a dropped big one.

    Three size-8 lines fill 192px. The fourth needs 64px and does not fit, so
    layout stops -- even though the trailing size-1 line would have fit in the
    remaining 48px. Preserving order beats filling space.
    """
    frame = Frame([Line("A", 8), Line("B", 8), Line("C", 8), Line("D", 8), Line("E", 1)])
    assert [(line.text, line.size) for line in frame.fit()] == [("A", 8), ("B", 8), ("C", 8)]


def test_wire_format_is_compact_ndjson() -> None:
    payload = Frame([Line("HI", 2)], wants_encoder=True).to_wire()
    assert payload.endswith(b"\n")
    assert b" " not in payload  # separators are tight, so frames stay small
    assert json.loads(payload) == {"e": 1, "l": [[2, "HI"]]}


def test_ascii_preview_is_a_size_one_grid() -> None:
    art = Frame([Line("HI", 8)]).to_ascii().splitlines()
    assert len(art) == rows_for(1) + 2  # plus top and bottom border
    assert all(len(row) == cols_for(1) + 2 for row in art)
    # A size-8 char occupies 8 columns, so "HI" lands at columns 0 and 8.
    assert art[1][1] == "H"
    assert art[1][9] == "I"


def test_rejects_out_of_range_text_size() -> None:
    with pytest.raises(ValueError, match="outside"):
        Line("x", size=9)
