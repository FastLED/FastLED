"""Read and edit the per-board project ini that fbuild consumes.

``ci/compiler/board_compiler.py`` generates ``<build_dir>/platformio.ini``
from ``Board.to_project_ini()`` and then patches its ``build_flags`` when
the staged sketch changes. This module is the small ini wrapper those two
steps share; fbuild parses the file itself, so nothing here resolves
platforms, frameworks or toolchains.
"""

import configparser
from pathlib import Path
from typing import Optional

from ci.util.global_interrupt_handler import handle_keyboard_interrupt


PROJECT_INI_NAME = "platformio.ini"


def _new_parser() -> configparser.ConfigParser:
    # Interpolation off: build_flags legitimately contain ``%`` and ``$``.
    return configparser.ConfigParser(interpolation=None)


class ProjectIni:
    """A parsed project ini with read/modify/write helpers.

    Use the factory methods:
    - ``ProjectIni.parseFile(path)``
    - ``ProjectIni.parseString(content)``
    - ``ProjectIni.create()``
    """

    config: configparser.ConfigParser
    file_path: Optional[Path]

    @staticmethod
    def parseFile(file_path: Path) -> "ProjectIni":
        """Parse an ini file from disk."""
        instance = object.__new__(ProjectIni)
        instance.config = _new_parser()
        instance.file_path = file_path
        if not file_path.exists():
            raise FileNotFoundError(f"project ini not found: {file_path}")
        instance.config.read(file_path, encoding="utf-8")
        return instance

    @staticmethod
    def parseString(content: str) -> "ProjectIni":
        """Parse ini content from a string."""
        instance = object.__new__(ProjectIni)
        instance.config = _new_parser()
        instance.file_path = None
        instance.config.read_string(content)
        return instance

    @staticmethod
    def create() -> "ProjectIni":
        """Create an empty ini."""
        instance = object.__new__(ProjectIni)
        instance.config = _new_parser()
        instance.file_path = None
        return instance

    def dump(self, file_path: Path) -> None:
        """Write the ini to ``file_path`` atomically."""
        temp_file = file_path.with_suffix(".tmp")
        try:
            with open(temp_file, "w", encoding="utf-8") as f:
                self.config.write(f)
            temp_file.replace(file_path)
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception:
            if temp_file.exists():
                temp_file.unlink()
            raise

    def get_sections(self) -> list[str]:
        return self.config.sections()

    def get_env_sections(self) -> list[str]:
        """Section names starting with ``env:``."""
        return [s for s in self.config.sections() if s.startswith("env:")]

    def has_section(self, section: str) -> bool:
        return self.config.has_section(section)

    def has_option(self, section: str, option: str) -> bool:
        return self.config.has_option(section, option)

    def get_option(
        self, section: str, option: str, fallback: Optional[str] = None
    ) -> Optional[str]:
        if not self.config.has_section(section):
            return fallback
        return self.config.get(section, option, fallback=fallback)

    def set_option(self, section: str, option: str, value: str) -> None:
        if not self.config.has_section(section):
            self.config.add_section(section)
        self.config.set(section, option, value)

    def remove_option(self, section: str, option: str) -> bool:
        return self.config.remove_option(section, option)

    def get_build_flags(self, env_name: str) -> list[str]:
        """``build_flags`` of ``[env:<env_name>]`` as one flag per entry."""
        raw = self.get_option(f"env:{env_name}", "build_flags", "") or ""
        return [line.strip() for line in raw.splitlines() if line.strip()]

    def set_build_flags(self, env_name: str, flags: list[str]) -> None:
        """Replace ``build_flags`` of ``[env:<env_name>]`` (one flag per line)."""
        self.set_option(f"env:{env_name}", "build_flags", "\n" + "\n".join(flags))

    def __str__(self) -> str:
        import io

        buf = io.StringIO()
        self.config.write(buf)
        return buf.getvalue()
