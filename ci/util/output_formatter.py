from __future__ import annotations

import re
import time
from typing import Protocol


class OutputFormatter(Protocol):
    """Protocol for output formatters used with RunningProcess."""

    def begin(self) -> None: ...

    def transform(self, line: str) -> str: ...

    def end(self) -> None: ...


class NullOutputFormatter:
    """No-op formatter that returns input unchanged and has no lifecycle effects."""

    def begin(self) -> None:
        return None

    def transform(self, line: str) -> str:
        return line

    def end(self) -> None:
        return None


class _PathSubstitutionFormatter:
    """Formatter that replaces sketch source paths and prefixes timestamps."""

    def __init__(self, needle: str, regex_pattern: str, replacement: str) -> None:
        self._needle: str = needle
        self._regex_pattern: str = regex_pattern
        self._replacement: str = replacement
        self._start_time: float = 0.0
        self._compiled: re.Pattern[str] | None = None

    def begin(self) -> None:
        self._start_time = time.time()

    def transform(self, line: str) -> str:
        if not line:
            return line
        formatted: str = self._format_paths(line)
        elapsed: float = time.time() - self._start_time
        return f"{elapsed:.2f} {formatted}"

    def end(self) -> None:
        return None

    def _format_paths(self, line: str) -> str:
        if self._needle not in line:
            return line
        if self._compiled is None:
            self._compiled = re.compile(self._regex_pattern)
        if self._compiled.search(line):
            return self._compiled.sub(self._replacement, line)
        return line


def create_sketch_path_formatter(example: str) -> OutputFormatter:
    """Create a formatter that maps src/sketch paths to examples/{example}/ and timestamps lines.

    Args:
        example: Example name or path (e.g., "Pintest" or "examples/SmartMatrix").

    Returns:
        OutputFormatter: Configured formatter instance.
    """
    # Normalize example path
    display_example_str: str
    if "/" in example or "\\" in example:
        display_example_str = example.replace("\\", "/")
    else:
        display_example_str = f"examples/{example}"

    return _PathSubstitutionFormatter(
        needle="sketch",
        regex_pattern=r"src[/\\]+sketch[/\\]+",
        replacement=f"{display_example_str}/",
    )
