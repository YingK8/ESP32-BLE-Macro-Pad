"""Configuration: a TOML file for preferences, environment for secrets."""

from __future__ import annotations

from pathlib import Path

from platformdirs import user_config_path
from pydantic import BaseModel, Field
from pydantic_settings import (
    BaseSettings,
    PydanticBaseSettingsSource,
    SettingsConfigDict,
    TomlConfigSettingsSource,
)

CONFIG_DIR = user_config_path("macropad-host")
CONFIG_PATH = CONFIG_DIR / "config.toml"

DEFAULT_CONFIG = """\
# macropad-host configuration.
# Secrets do NOT belong here -- see the note under [agenda].

apps = ["pomodoro", "agenda", "nowplaying", "sysstats"]

[link]
port = "auto"        # "auto" picks the first Espressif USB CDC port

[pomodoro]
work_min = 25
break_min = 5
long_break_min = 15
sessions_before_long = 4
focus_shortcut = ""  # name of a macOS Shortcut to run on work start/stop; "" disables

[agenda]
ics_urls = []        # e.g. your Google Calendar "secret address in iCal format"
canvas_url = "https://bcourses.berkeley.edu"
lookahead_days = 7
# The Canvas token is read from the MACROPAD_CANVAS_TOKEN environment variable
# (or host/.env), never from this file -- this file is easy to leak into git.
"""


class LinkSettings(BaseModel):
    port: str = "auto"
    baudrate: int = 115200


class PomodoroSettings(BaseModel):
    work_min: int = 25
    break_min: int = 5
    long_break_min: int = 15
    sessions_before_long: int = 4
    focus_shortcut: str = ""


class AgendaSettings(BaseModel):
    ics_urls: list[str] = Field(default_factory=list)
    canvas_url: str = "https://bcourses.berkeley.edu"
    lookahead_days: int = 7


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_prefix="MACROPAD_",
        env_file=".env",
        env_file_encoding="utf-8",
        toml_file=CONFIG_PATH,
        extra="ignore",
    )

    apps: list[str] = Field(default_factory=lambda: ["pomodoro", "agenda", "nowplaying", "sysstats"])
    canvas_token: str = ""  # MACROPAD_CANVAS_TOKEN

    link: LinkSettings = Field(default_factory=LinkSettings)
    pomodoro: PomodoroSettings = Field(default_factory=PomodoroSettings)
    agenda: AgendaSettings = Field(default_factory=AgendaSettings)

    @classmethod
    def settings_customise_sources(
        cls,
        settings_cls: type[BaseSettings],
        init_settings: PydanticBaseSettingsSource,
        env_settings: PydanticBaseSettingsSource,
        dotenv_settings: PydanticBaseSettingsSource,
        file_secret_settings: PydanticBaseSettingsSource,
    ) -> tuple[PydanticBaseSettingsSource, ...]:
        # Env beats the TOML file, so a secret can always override a committed value.
        return (init_settings, env_settings, dotenv_settings, TomlConfigSettingsSource(settings_cls))


def ensure_config(path: Path = CONFIG_PATH) -> Path:
    """Write the commented default config on first run. Returns the path."""
    if not path.exists():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(DEFAULT_CONFIG)
    return path


def load() -> Settings:
    ensure_config()
    return Settings()
