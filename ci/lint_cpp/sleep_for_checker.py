#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker for sleep_for() usage - DANGEROUS: blocks async/scheduler pumping.

⚠️  CRITICAL: sleep_for() is a bare OS-level sleep that BLOCKS all task pumping.
This causes:
  - Async operations (HTTP, promises) to hang and not complete
  - Background tasks (fl::task) to freeze and not run
  - Unresponsive UI/effects during sleep periods
  - Potential deadlocks if async code is waiting

ALWAYS use fl::delay(ms) instead, which keeps everything responsive.

sleep_for() may ONLY be used in core infrastructure (threading, timing primitives)
where you explicitly need a blocking sleep. Any other use is a BUG.

Suppression: Requires '// ok sleep for' comment to explicitly acknowledge this risk.
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
    "platforms/stub/coroutine_stub.hpp",
)

# Match sleep_for with any prefix (std::this_thread::, fl::this_thread::, or bare)
_SLEEP_FOR_RE = re.compile(r"\bsleep_for\s*\(")


class SleepForChecker(FileContentChecker):
    """Checker for sleep_for() usage - pure low-level OS sleep with no pumping."""

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

            # Fast first pass: skip regex if "sleep_for" not in line
            if "sleep_for" not in code_part:
                continue

            # Check for sleep_for usage
            if _SLEEP_FOR_RE.search(code_part):
                violations.append(
                    (
                        line_number,
                        f"⚠️  CRITICAL: sleep_for() BLOCKS async/scheduler pumping! "
                        f"Async operations HANG, tasks FREEZE, UI becomes UNRESPONSIVE. "
                        f"USE fl::delay(ms) INSTEAD! "
                        f"Only suppress with '// ok sleep for' in core infrastructure: "
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
        "⚠️  CRITICAL: Found sleep_for() usage that BLOCKS async/scheduler! Use fl::delay(ms) INSTEAD!",
    )


if __name__ == "__main__":
    main()
