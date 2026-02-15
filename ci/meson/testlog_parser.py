"""Parse meson testlog.txt to extract detailed error information from failed tests."""

import re
from collections import deque
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

from ci.util.timestamp_print import ts_print as _ts_print


@dataclass
class TestLogEntry:
    """Parsed test log entry from testlog.txt"""

    test_name: str
    result: str  # "exit status 0" or "exit status 1", etc.
    duration: str
    stdout: str
    stderr: str


def parse_testlog(testlog_path: Path) -> list[TestLogEntry]:
    """
    Parse meson testlog.txt into structured test entries.

    Args:
        testlog_path: Path to testlog.txt file

    Returns:
        List of TestLogEntry objects, one per test
    """
    if not testlog_path.exists():
        return []

    with open(testlog_path, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()

    # Split into test sections using the separator pattern
    # Pattern matches: "=================================== N/M ==================================="
    section_pattern = r"={30,}\s+\d+/\d+\s+={30,}"
    sections = re.split(section_pattern, content)

    entries: list[TestLogEntry] = []

    for section in sections:
        if not section.strip():
            continue

        # Extract test name
        test_match = re.search(r"test:\s+(\S+)", section)
        if not test_match:
            continue

        test_name = test_match.group(1)

        # Extract result
        result_match = re.search(r"result:\s+(.+)", section)
        result = result_match.group(1) if result_match else "unknown"

        # Extract duration
        duration_match = re.search(r"duration:\s+([\d.]+s)", section)
        duration = duration_match.group(1) if duration_match else "N/A"

        # Extract stdout section (between "--- stdout ---" and "--- stderr ---" or next section)
        stdout_match = re.search(
            r"-+ stdout -+\n(.*?)(?:-+ stderr -+|$)", section, re.DOTALL
        )
        stdout = stdout_match.group(1).strip() if stdout_match else ""

        # Extract stderr section (between "--- stderr ---" and next section or end)
        # Note: Use negative lookahead to avoid matching box-drawing characters (═)
        # Only match the actual section separator: ={30,} at start of line
        stderr_match = re.search(
            r"-+ stderr -+\n(.*?)(?=\n={30,}|\Z)", section, re.DOTALL
        )
        stderr = stderr_match.group(1).strip() if stderr_match else ""

        entries.append(
            TestLogEntry(
                test_name=test_name,
                result=result,
                duration=duration,
                stdout=stdout,
                stderr=stderr,
            )
        )

    return entries


def extract_error_context_from_testlog(
    testlog_path: Path,
    failed_test_names: set[str],
    context_lines: int = 10,
) -> None:
    """
    Parse testlog.txt and display error context for failed tests.

    Args:
        testlog_path: Path to meson testlog.txt
        failed_test_names: Set of test names that failed (e.g., {"fastled:fl_remote_remote"})
        context_lines: Number of lines to show before/after error lines
    """
    entries = parse_testlog(testlog_path)

    # Filter to only failed tests
    failed_entries = [
        e
        for e in entries
        if e.test_name in failed_test_names and "exit status 0" not in e.result
    ]

    if not failed_entries:
        _ts_print("[MESON] ⚠️  Could not find detailed error information in testlog.txt")
        return

    _ts_print("\n[MESON] ❌ Test failure details from testlog.txt:")
    _ts_print("=" * 80)

    for entry in failed_entries:
        _ts_print(f"\n[TEST FAILED] {entry.test_name} ({entry.duration})")
        _ts_print(f"[EXIT CODE] {entry.result}")
        _ts_print("-" * 80)

        # Combine stdout and stderr for error detection
        combined_output: list[str] = []
        if entry.stdout:
            combined_output.append("=== STDOUT ===")
            combined_output.extend(entry.stdout.splitlines())
        if entry.stderr:
            combined_output.append("=== STDERR ===")
            combined_output.extend(entry.stderr.splitlines())

        if not combined_output:
            _ts_print("(No output captured)")
            continue

        # Apply error detection with context
        error_filter: Callable[[str], None] = _create_error_context_filter(
            context_lines=context_lines,
            max_unique_errors=10,  # Show up to 10 unique errors per test
        )

        line: str
        for line in combined_output:
            error_filter(line)

    _ts_print("=" * 80)


def _create_error_context_filter(
    context_lines: int = 10,
    max_unique_errors: int = 10,
) -> Callable[[str], None]:
    """
    Create a filter function that detects error lines and shows context.

    This is similar to the one in compile.py but specialized for test output.

    Args:
        context_lines: Number of lines to show before and after error detection
        max_unique_errors: Maximum unique errors to show (0 = unlimited)

    Returns:
        Filter function that takes a line and returns None (consumes line)
    """
    # Circular buffer for context before errors
    pre_error_buffer: deque[str] = deque(maxlen=context_lines)

    # Counter for lines after error (to show context after error)
    post_error_lines = 0

    # Track if we've seen any errors
    error_detected = False

    # Track seen error signatures to avoid showing identical errors repeatedly
    seen_errors: set[str] = set()

    # Error patterns for test failures (case-insensitive)
    error_patterns = [
        "error:",
        "failed",
        "failure",
        "FAILED",
        "ERROR",
        "ASSERT",
        "assertion",
        "segmentation fault",
        "core dumped",
        "abort",
        "exception",
        "fatal",
    ]

    def filter_line(line: str) -> None:
        """Process a line and print it if it's part of error context."""
        nonlocal post_error_lines, error_detected

        # Check if this line contains an error pattern
        line_lower = line.lower()
        is_error_line = any(pattern.lower() in line_lower for pattern in error_patterns)

        if is_error_line:
            # Extract error signature (first 100 chars for deduplication)
            error_sig = line_lower[:100]

            # Check if we've seen this exact error before
            is_new_error = error_sig not in seen_errors
            if is_new_error:
                seen_errors.add(error_sig)

            # Only show first N unique errors to prevent truncation
            should_show = (
                max_unique_errors == 0 or len(seen_errors) <= max_unique_errors
            )

            if should_show:
                # Error detected! Output all buffered pre-context
                if not error_detected:
                    error_detected = True

                # Output buffered pre-context
                for buffered_line in pre_error_buffer:
                    _ts_print(f"  {buffered_line}")

                # Output this error line with red color highlighting
                _ts_print(f"  \033[91m{line}\033[0m")

                # Start counting post-error lines
                post_error_lines = context_lines

            return  # Don't buffer this line (already handled)

        # If we're in post-error context, show the line
        if post_error_lines > 0:
            _ts_print(f"  {line}")
            post_error_lines -= 1
            return  # Don't buffer (already shown)

        # Not an error line and not in post-error context
        # Add to circular buffer for potential future context
        pre_error_buffer.append(line)

    return filter_line
