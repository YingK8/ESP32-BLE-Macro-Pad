"""Text frames and the character-grid maths shared with the firmware.

The device is a dumb character-cell terminal: we send it finished lines of text
and it blits them. All wrapping and clipping happens here, on the host, because
the host is the side that knows the panel geometry and has stdlib textwrap.
"""

from __future__ import annotations

import json
import textwrap
from dataclasses import dataclass, field

# Panel geometry after TFT_ROTATION=1 on the 240x280 ST7789 (physical portrait
# panel, used in landscape). Must match TFT_WIDTH/TFT_HEIGHT in include/Config.h.
PANEL_W = 280
PANEL_H = 240

# Arduino_GFX's built-in font is a 5x7 glyph drawn in a 6x8 px cell. setTextSize(s)
# is an integer pixel multiplier, so a size-s character occupies 6s x 8s pixels.
CELL_W = 6
CELL_H = 8

MIN_SIZE = 1
MAX_SIZE = 8


def cols_for(size: int) -> int:
    """How many characters of the given text size fit across the panel."""
    return PANEL_W // (CELL_W * size)


def rows_for(size: int) -> int:
    """How many rows of the given text size fit down the panel."""
    return PANEL_H // (CELL_H * size)


def line_height(size: int) -> int:
    """Pixel height consumed by one line at the given text size."""
    return CELL_H * size


@dataclass(frozen=True)
class Line:
    """One logical line of text. `size` is the Arduino_GFX text multiplier.

    `indent` is prefixed to wrapped continuation rows, so a list item that spills
    over stays visually attached to its bullet instead of reading as a new entry.
    """

    text: str
    size: int = 2
    indent: str = ""

    def __post_init__(self) -> None:
        if not MIN_SIZE <= self.size <= MAX_SIZE:
            raise ValueError(f"text size {self.size} outside {MIN_SIZE}..{MAX_SIZE}")


@dataclass
class Frame:
    """A full screen of text plus the input mode the active app wants.

    `wants_encoder` asks the firmware to forward encoder deltas to us instead of
    emitting HID volume keys. It travels with the frame so the mode can never
    drift out of sync with what is on screen.
    """

    lines: list[Line] = field(default_factory=list)
    wants_encoder: bool = False

    def add(self, text: str, size: int = 2, indent: str = "") -> "Frame":
        """Append a line; returns self so apps can chain calls."""
        self.lines.append(Line(text, size, indent))
        return self

    def fit(self) -> list[Line]:
        """Wrap every line to its size's column count and clip to the panel height.

        Lines are laid out top-down and the first one that would overflow stops
        the layout -- a partially drawn line is worse than a missing one.
        """
        out: list[Line] = []
        used = 0
        for line in self.lines:
            width = cols_for(line.size)
            # textwrap.wrap returns [] for whitespace-only input, but a blank line
            # is a deliberate spacer, so preserve it as one empty row.
            pieces = textwrap.wrap(
                line.text,
                width=width,
                subsequent_indent=line.indent,
                break_long_words=True,
                break_on_hyphens=False,
            ) or [""]
            for piece in pieces:
                height = line_height(line.size)
                if used + height > PANEL_H:
                    return out
                out.append(Line(piece, line.size))
                used += height
        return out

    def to_wire(self) -> bytes:
        """Encode as one newline-delimited JSON object, ready to write to serial.

        Keys are single characters to keep frames small: `e` = wants_encoder,
        `l` = list of [size, text] pairs.
        """
        payload = {
            "e": 1 if self.wants_encoder else 0,
            "l": [[line.size, line.text] for line in self.fit()],
        }
        return json.dumps(payload, ensure_ascii=True, separators=(",", ":")).encode() + b"\n"

    def to_ascii(self) -> str:
        """Render the frame as a size-1 character grid, for --dry-run previews.

        Each size-s character is expanded to an s x s block of cells, so the
        preview reflects the real relative sizes rather than just the text.
        """
        grid_w = cols_for(1)
        grid_h = rows_for(1)
        canvas = [[" "] * grid_w for _ in range(grid_h)]

        row = 0
        for line in self.fit():
            for col, char in enumerate(line.text):
                x, y = col * line.size, row
                if y < grid_h and x < grid_w:
                    canvas[y][x] = char
            row += line.size

        border = "+" + "-" * grid_w + "+"
        body = "\n".join("|" + "".join(r) + "|" for r in canvas)
        return f"{border}\n{body}\n{border}"
