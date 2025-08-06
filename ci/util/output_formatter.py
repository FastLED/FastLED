#!/usr/bin/env python3
"""Abstract output formatter for RunningProcess."""

import time
from abc import ABC, abstractmethod
from typing import Protocol


class OutputFormatter(Protocol):
    """Protocol for output formatters that can be used with RunningProcess."""

    def begin(self) -> None:
        """Called when output processing begins. Starts internal timing."""
        ...

    def transform(self, line: str) -> str:
        """Transform a single line of output.

        Args:
            line: Raw line from the process output

        Returns:
            Transformed line
        """
        ...

    def end(self) -> None:
        """Called when output processing ends. For completeness."""
        ...


class TimestampFormatter:
    """Basic formatter that adds timestamps to each line."""

    def __init__(self) -> None:
        self._start_time: float = 0.0

    def begin(self) -> None:
        """Start the internal timer."""
        self._start_time = time.time()

    def transform(self, line: str) -> str:
        """Add timestamp to the line and return it."""
        if not line.strip():
            return line

        elapsed = time.time() - self._start_time
        return f"{elapsed:.2f} {line}"

    def end(self) -> None:
        """End processing (no-op for basic formatter)."""
        pass


class PathSubstitutionFormatter:
    """Generic formatter that performs configurable path substitution and adds timestamps."""

    def __init__(self, needle: str, regex_pattern: str, replacement: str) -> None:
        """Initialize the path substitution formatter.

        Args:
            needle: Fast string to check if line contains before applying regex (e.g., "sketch")
            regex_pattern: Regex pattern to match for substitution (e.g., r'src[/\\\\]sketch[/\\\\]')
            replacement: Replacement string (e.g., "examples/Pintest/")
        """
        self.needle = needle
        self.regex_pattern = regex_pattern
        self.replacement = replacement
        self._start_time: float = 0.0
        self._regex_compiled = None  # Lazy compilation

    def begin(self) -> None:
        """Start the internal timer."""
        self._start_time = time.time()

    def transform(self, line: str) -> str:
        """Format paths in the line and add timestamp."""
        if not line.strip():
            return line

        formatted_line = self._format_paths(line)
        elapsed = time.time() - self._start_time
        return f"{elapsed:.2f} {formatted_line}"

    def end(self) -> None:
        """End processing (no-op for this formatter)."""
        pass

    def _format_paths(self, line: str) -> str:
        """Format paths in a single line using configured needle and regex."""
        # Fast pre-check to avoid expensive regex operations
        if self.needle not in line:
            return line

        # Lazy compile regex pattern
        if self._regex_compiled is None:
            import re

            self._regex_compiled = re.compile(self.regex_pattern)

        # Apply substitution if pattern matches
        if self._regex_compiled.search(line):
            return self._regex_compiled.sub(self.replacement, line)

        return line


class NoOpFormatter:
    """Formatter that does nothing - just prints lines as-is."""

    def begin(self) -> None:
        """No-op begin."""
        pass

    def transform(self, line: str) -> str:
        """Return line as-is."""
        return line

    def end(self) -> None:
        """No-op end."""
        pass


def create_sketch_path_formatter(example: str) -> PathSubstitutionFormatter:
    """Create a PathSubstitutionFormatter configured for src/sketch -> examples path substitution.

    Args:
        example: Example name (e.g., "Pintest", "examples/SmartMatrix")

    Returns:
        Configured PathSubstitutionFormatter for sketch path substitution
    """
    # Determine display example path
    if "/" in example or "\\\\" in example:
        # Already a path-like string, use as-is but normalize
        display_example_str = example.replace("\\\\", "/")
    else:
        # Simple name, convert to examples/{name}
        display_example_str = f"examples/{example}"

    return PathSubstitutionFormatter(
        needle="sketch",
        regex_pattern=r"src[/\\\\]+sketch[/\\\\]+",
        replacement=f"{display_example_str}/",
    )
