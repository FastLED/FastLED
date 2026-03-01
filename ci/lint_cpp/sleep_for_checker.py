#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker for sleep_for() usage - bypasses async runner.

Detects usage of sleep_for() in any namespace (std::this_thread::sleep_for,
fl::this_thread::sleep_for, or bare sleep_for). Code should use fl::yield()
or fl::async_run() instead, which cooperate with the async runner.

Suppression: add '// ok sleep for' or '// okay sleep for' to the line.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker, is_excluded_file


# Files that legitimately need sleep_for (thread/time infrastructure)
_WHITELISTED_SUFFIXES: tuple[str, ...] = (
    # Thread implementation (defines sleep_for)
    "fl/stl/thread.h",
    # Platform thread implementations (bridge fl:: -> std::)
    "platforms/stub/thread_stub_stl.h",
    "platforms/stub/thread_stub_noop.h",
    # Platform time (implements delay/delayMicroseconds)
    "platforms/stub/platform_time.cpp.hpp",
    # Async runner itself (needs raw sleep to yield CPU)
    "fl/async.cpp.hpp",
    # Host timer (needs precise std:: sleep for SPI bitbang timing)
    "platforms/shared/spi_bitbang/host_timer.cpp.hpp",
    # Task coroutine stub (cooperative scheduling infrastructure)
    "platforms/stub/task_coroutine_stub.cpp.hpp",
)

# Match sleep_for with any prefix (std::this_thread::, fl::this_thread::, or bare)
_SLEEP_FOR_RE = re.compile(r"\bsleep_for\s*\(")


class SleepForChecker(FileContentChecker):
    """Checker for sleep_for() usage that bypasses async runner."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".cpp.hpp")):
            return False

        if is_excluded_file(file_path):
            return False

        if "third_party" in file_path or "thirdparty" in file_path:
            return False

        # Exclude infrastructure files that must use sleep_for
        normalized = file_path.replace("\\", "/")
        if any(normalized.endswith(suffix) for suffix in _WHITELISTED_SUFFIXES):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for sleep_for() usage."""
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track multi-line comment state
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue

            if in_multiline_comment:
                continue

            if stripped.startswith("//"):
                continue

            # Remove single-line comment portion
            code_part = line.split("//")[0]

            # Skip lines with suppression comment
            if "// ok sleep for" in line or "// okay sleep for" in line:
                continue

            # Check for sleep_for usage
            if _SLEEP_FOR_RE.search(code_part):
                violations.append(
                    (
                        line_number,
                        f"sleep_for() bypasses async runner — use fl::yield() or "
                        f"fl::async_run() instead. Suppress with '// ok sleep for': "
                        f"{stripped}",
                    )
                )

        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run sleep_for checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = SleepForChecker()
    run_checker_standalone(
        checker,
        [
            str(PROJECT_ROOT / "src"),
            str(PROJECT_ROOT / "tests"),
            str(PROJECT_ROOT / "examples"),
        ],
        "Found sleep_for() usage (bypasses async runner — use fl::yield/fl::async_run)",
    )


if __name__ == "__main__":
    main()
