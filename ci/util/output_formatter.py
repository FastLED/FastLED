from __future__ import annotations

import re
import time
from collections.abc import Callable

from running_process import OutputFormatter


# Patterns to suppress in verbose mode (noise that adds no debugging value)
VERBOSE_SUPPRESS_PATTERNS: list[re.Pattern[str]] = [
    # Ninja internal messages
    re.compile(r"ninja: Entering directory", re.IGNORECASE),
    re.compile(r"ninja: no work to do\.$", re.IGNORECASE),
    # Meson backend internals
    re.compile(r"INFO:\s*autodetecting backend", re.IGNORECASE),
    re.compile(r"INFO:\s*calculating backend command", re.IGNORECASE),
    # Sanitizer environment dump (massive line with env vars like ASAN_OPTIONS, MSAN_OPTIONS etc)
    # This matches lines starting with >>> followed by environment variables
    re.compile(r">>>\s*[A-Z_]+=.*[A-Z_]+="),
    # Windows crash handler setup (always appears, never actionable)
    # Note: may have timestamp prefix like "0.01 " from formatter
    re.compile(r"(?:^\d+\.\d+\s+)?Setting up Windows crash handler"),
    re.compile(r"(?:^\d+\.\d+\s+)?Windows crash handler setup complete"),
    # RunningProcess internal draining message
    re.compile(r"\[Drained \d+ final lines"),
    # CoroutineRunner internal initialization
    re.compile(r"(?:^\d+\.\d+\s+)?Pre-initializing CoroutineRunner"),
    re.compile(r"(?:^\d+\.\d+\s+)?CoroutineRunner singleton pre-initialized"),
    # Meson scissor line decorators (Unicode horizontal bar U+2015, scissors U+2700/✀)
    # Matches lines like "―――――――――――――――――――――――――――― ✀  ―――――――――――――――――――――――――――――"
    re.compile(r"\u2015{10,}.*[\u2700\u2702]"),
    # Full horizontal lines (separator noise - Unicode horizontal bar U+2015)
    # Matches lines with only horizontal bars (with optional timestamp prefix from formatter)
    re.compile(r"^\d+\.\d+\s+\u2015{40,}\s*$"),
    re.compile(r"^\u2015{40,}\s*$"),
    # Full log path message (only useful on failure, not in passing output)
    re.compile(r"Full log written to"),
    # Meson test summary lines (already shown in final summary)
    # Matches lines like "1.29 Ok:                1" and "1.29 Fail:              0"
    # Note: removed trailing $ anchor to handle trailing whitespace/content
    re.compile(r"^\d+\.\d+\s+Ok:\s+\d+"),
    re.compile(r"^\d+\.\d+\s+Fail:\s+\d+"),
    # Doctest boilerplate (verbose noise with no debugging value)
    re.compile(r"\[doctest\] doctest version is"),
    re.compile(r'\[doctest\] run with "--help"'),
    # Doctest separator lines (with optional timestamp prefix)
    re.compile(r"^\d+\.\d+\s+={50,}\s*$"),
    # Meson test "RUNNING" status lines (only show results, not running status)
    # Matches lines like "0.66 1/1 examples - fastled:example-Blink RUNNING"
    re.compile(r"^\d+\.\d+\s+\d+/\d+.*RUNNING$"),
    # Meson example test "OK" output lines (internal format leaking through)
    # Matches lines like "0.75 1/1 examples - fastled:example-Blink OK               0.02s"
    # This info is redundant - success indicator already shown in timing summary
    re.compile(
        r"^\d+\.\d+\s+\d+/\d+\s+examples\s+-\s+fastled:example-.*OK\s+\d+\.\d+s"
    ),
    # Doctest file:line reference lines (noise without context)
    # Matches lines like "0.01 ../../tests/fl/xypath.cpp:50:" (standalone file refs)
    re.compile(r"^\d+\.\d+\s+\.\./\.\./.*:\d+:\s*$"),
    # Doctest MESSAGE lines (debug output with any content)
    # Matches lines like "0.01 ../../tests/fl/xypath.cpp:50: MESSAGE: some value"
    re.compile(r"^\d+\.\d+\s+.*:\d+:\s*MESSAGE:"),
    # Doctest INFO() output lines (debug print values without MESSAGE prefix)
    # These are multi-line debug values from INFO() statements
    # Keep only TEST CASE lines and final summary, filter everything else that looks like debug output
    re.compile(r"^\d+\.\d+\s+Tile:\s*$"),
    re.compile(
        r"^\d+\.\d+\s+[\s\d]+$"
    ),  # Lines with only spaces and numbers (matrix values)
    re.compile(
        r"^\d+\.\d+\s+\w+ shape bounds:\s*$"
    ),  # "Heart shape bounds:", "Spiral shape bounds:"
    re.compile(
        r"^\d+\.\d+\s+\d+-petal rose shape bounds:\s*$"
    ),  # "3-petal rose shape bounds:"
    re.compile(
        r"^\d+\.\d+\s+\s+\d+-petal rose\s*$"
    ),  # "  3-petal rose" (indented subcase)
]


def should_suppress_line(line: str) -> bool:
    """Check if a line should be suppressed in verbose output.

    Args:
        line: The line to check

    Returns:
        True if the line should be suppressed, False otherwise
    """
    return any(pattern.search(line) for pattern in VERBOSE_SUPPRESS_PATTERNS)


class TimestampFormatter:
    """Simple formatter that adds timestamps relative to program start."""

    def __init__(self) -> None:
        self._start_time: float = 0.0

    def begin(self) -> None:
        self._start_time = time.time()

    def transform(self, line: str) -> str:
        if not line:
            return line
        elapsed: float = time.time() - self._start_time
        return f"{elapsed:.2f} {line}"

    def end(self) -> None:
        return None


class FilteringTimestampFormatter:
    """Formatter that adds timestamps and filters out verbose noise patterns.

    This formatter suppresses common noise patterns from meson, ninja, and
    internal tools that clutter verbose output without providing useful
    debugging information.
    """

    def __init__(self) -> None:
        self._start_time: float = 0.0

    def begin(self) -> None:
        self._start_time = time.time()

    def transform(self, line: str) -> str | None:
        if not line:
            return line
        # Check if this line should be suppressed
        if should_suppress_line(line):
            return None  # Signal to skip this line
        elapsed: float = time.time() - self._start_time
        return f"{elapsed:.2f} {line}"

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


def create_filtering_echo_callback() -> Callable[[str], None]:
    """Create an echo callback that filters out verbose noise patterns.

    This callback can be passed to RunningProcess.wait(echo=callback) to
    suppress common noise patterns from meson, ninja, and internal tools.

    Returns:
        A callable that prints lines unless they match noise patterns.
    """

    def filtering_echo(line: str) -> None:
        """Echo line to stdout unless it matches a suppress pattern."""
        if not should_suppress_line(line):
            print(line)

    return filtering_echo


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
