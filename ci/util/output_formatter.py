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


class _MultiPathSubstitutionFormatter:
    """Formatter that applies multiple path replacements and prefixes timestamps."""

    def __init__(self, substitutions: list[tuple[str, str, str]]) -> None:
        """Initialize with list of (needle, regex_pattern, replacement) tuples."""
        self._substitutions: list[tuple[str, str, re.Pattern[str]]] = []
        for needle, regex_pattern, replacement in substitutions:
            compiled_pattern = re.compile(regex_pattern)
            self._substitutions.append((needle, replacement, compiled_pattern))
        self._start_time: float = 0.0

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
        result = line
        for needle, replacement, compiled_pattern in self._substitutions:
            if needle in result and compiled_pattern.search(result):
                result = compiled_pattern.sub(replacement, result)
        return result


def create_sketch_path_formatter(example: str) -> OutputFormatter:
    """Create a formatter that maps lib/FastLED paths to src/ and src/sketch paths to examples/{example}/ and timestamps lines.

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

    # Define multiple path substitutions
    substitutions = [
        # Replace lib/FastLED/ paths with src/ for better UX
        ("lib/FastLED", r"lib[/\\]+FastLED[/\\]+", "src/"),
        # Replace src/sketch/ paths with examples/{example}/
        ("sketch", r"src[/\\]+sketch[/\\]+", f"{display_example_str}/"),
    ]

    return _MultiPathSubstitutionFormatter(substitutions)
