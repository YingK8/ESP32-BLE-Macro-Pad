"""Next class/meeting from .ics feeds, merged with bCourses assignment deadlines."""

from __future__ import annotations

import datetime as dt
import logging
import webbrowser

import httpx
import icalendar
import recurring_ical_events

from ..api import KEY_ACTION_A, BaseApp, Frame, InputEvent

log = logging.getLogger(__name__)


def _as_datetime(value) -> dt.datetime:  # noqa: ANN001 - icalendar yields date or datetime
    """Normalise to an aware local datetime. All-day events land at local midnight.

    .astimezone() converts an aware datetime and assumes local time for a naive
    one, which is the behaviour we want in both cases.
    """
    if not isinstance(value, dt.datetime):
        value = dt.datetime.combine(value, dt.time.min)
    return value.astimezone()


def _relative(when: dt.datetime, now: dt.datetime) -> str:
    """Compact countdown: NOW, 42M, 3H, 2D."""
    delta = (when - now).total_seconds()
    if delta <= 0:
        return "NOW"
    if delta < 3600:
        return f"{int(delta // 60)}M"
    if delta < 86400:
        return f"{int(delta // 3600)}H"
    return f"{int(delta // 86400)}D"


class AgendaApp(BaseApp):
    name = "agenda"
    refresh_seconds = 30.0
    data_interval_seconds = 300.0  # network work stays well off the render path
    wants_encoder = True

    def __init__(self, settings, canvas_token: str) -> None:  # noqa: ANN001 - config.AgendaSettings
        self._cfg = settings
        self._token = canvas_token
        self._items: list[dict] = []
        self._error = ""
        self._selected = 0

    # -- data --------------------------------------------------------------

    def refresh_data(self) -> None:
        now = dt.datetime.now().astimezone()
        horizon = now + dt.timedelta(days=self._cfg.lookahead_days)
        items: list[dict] = []
        errors: list[str] = []

        for url in self._cfg.ics_urls:
            try:
                items.extend(self._fetch_ics(url, now, horizon))
            except Exception as exc:  # noqa: BLE001 - one bad feed must not blank the screen
                log.warning("ics %s failed: %s", url, exc)
                errors.append("ICS")

        if self._token:
            try:
                items.extend(self._fetch_canvas(now, horizon))
            except Exception as exc:  # noqa: BLE001
                log.warning("canvas failed: %s", exc)
                errors.append("CANVAS")

        items.sort(key=lambda i: i["when"])
        self._items = items
        self._error = " ".join(errors)

    def _fetch_ics(self, url: str, start: dt.datetime, end: dt.datetime) -> list[dict]:
        text = httpx.get(url, follow_redirects=True, timeout=15).raise_for_status().text
        calendar = icalendar.Calendar.from_ical(text)
        # recurring_ical_events expands RRULE/EXDATE for us -- the part of iCalendar
        # you really do not want to reimplement.
        out = []
        for event in recurring_ical_events.of(calendar).between(start, end):
            out.append(
                {
                    "when": _as_datetime(event["DTSTART"].dt),
                    "title": str(event.get("SUMMARY", "(untitled)")),
                    "url": str(event.get("URL", "")) or "",
                }
            )
        return out

    def _fetch_canvas(self, start: dt.datetime, end: dt.datetime) -> list[dict]:
        from canvasapi import Canvas  # imported lazily: only needed when a token is set

        canvas = Canvas(self._cfg.canvas_url, self._token)
        out = []
        for course in canvas.get_courses(enrollment_state="active"):
            code = getattr(course, "course_code", "") or ""
            try:
                assignments = course.get_assignments(bucket="upcoming")
            except Exception as exc:  # noqa: BLE001 - concluded courses 401 here
                log.debug("course %s assignments unavailable: %s", code, exc)
                continue
            for assignment in assignments:
                due = getattr(assignment, "due_at", None)
                if not due:
                    continue
                when = dt.datetime.fromisoformat(due.replace("Z", "+00:00")).astimezone()
                if not start <= when <= end:
                    continue
                out.append(
                    {
                        "when": when,
                        "title": f"{code} {assignment.name}".strip(),
                        "url": getattr(assignment, "html_url", "") or "",
                    }
                )
        return out

    # -- app surface -------------------------------------------------------

    def render(self) -> Frame:
        now = dt.datetime.now().astimezone()
        frame = Frame(wants_encoder=True)

        if not self._items:
            frame.add("AGENDA", size=2)
            frame.add("NOTHING DUE" if not self._error else f"NO DATA ({self._error})", size=3)
            if not self._cfg.ics_urls and not self._token:
                frame.add("", size=1)
                frame.add("Set ics_urls or MACROPAD_CANVAS_TOKEN", size=1)
            return frame

        self._selected %= len(self._items)
        head = self._items[0]
        # The soonest item gets the headline treatment; the rest are a scannable list.
        # ">" marks the encoder selection, which can also be the headline item.
        cursor = ">" if self._selected == 0 else " "
        frame.add(f"{cursor}NEXT IN {_relative(head['when'], now)}", size=2)
        frame.add(head["title"], size=3)
        frame.add("", size=1)

        for i, item in enumerate(self._items[1:], start=1):
            marker = ">" if i == self._selected else " "
            frame.add(f"{marker}{_relative(item['when'], now):>3} {item['title']}", size=2, indent="     ")
        return frame

    def on_input(self, event: InputEvent) -> None:
        if not self._items:
            return
        if event.kind == "encoder":
            self._selected = (self._selected + event.delta) % len(self._items)
        elif event.kind == "key" and event.down and event.key == KEY_ACTION_A:
            url = self._items[self._selected % len(self._items)]["url"]
            if url:
                webbrowser.open(url)
