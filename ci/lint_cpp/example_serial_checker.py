#!/usr/bin/env python3
"""Checker that forbids raw `Serial.print*` / `Serial.read*` / `Serial.begin` in
selected example sketches.

FastLED provides platform-aware wrappers in `fl::` (see `src/fl/stl/cstdio.h` and
`src/platforms/arduino/io_arduino.hpp`) that:
  - Skip writes when no host is connected (`if (!Serial) return;`)
  - Set `Serial.setTxTimeoutMs(0)` so HWCDC writes drop instead of stalling
  - Guard `flush()` against arduino-esp32 #7554 hangs

Sketches that bypass these wrappers by calling `Serial.print` directly lose the
non-blocking guarantees and the platform-specific fixes (e.g. the ESP32-C6 HWCDC
fixes shipped in #2669). To keep `AutoResearch.ino` aligned with the platform
contract — and to prevent regressions — this checker forbids the raw calls in
that example.

Banned patterns (in enforced files):
  - `Serial.print(...)`, `Serial.println(...)`, `Serial.printf(...)`
  - `Serial.write(...)`, `Serial.read(...)`, `Serial.available()`, `Serial.peek()`
  - `Serial.readStringUntil(...)`, `Serial.flush(...)`, `Serial.begin(...)`

Allowed (no `fl::` equivalent — platform-specific hardware config):
  - `Serial.setTxBufferSize(...)`, `Serial.setTxTimeoutMs(...)`

Status checks like `(bool)Serial` and `!Serial` are allowed; `fl::serial_ready()`
is the wrapper but the raw form is the idiomatic Arduino spelling.

Opt-out:
  - Append `// ok serial - <reason>` on the same line for genuinely-needed cases.
"""

import re
import sys
from pathlib import Path

from ci.util.check_files import (
    FileContent,
    FileContentChecker,
    MultiCheckerFileProcessor,
    run_checker_standalone,
)
from ci.util.paths import PROJECT_ROOT


EXAMPLES_ROOT = PROJECT_ROOT / "examples"

# Files where the rule is enforced. Path-prefix matches (POSIX-style).
# Add more example sketches here as they're migrated to fl::.
ENFORCED_PATH_PREFIXES = ("examples/AutoResearch/",)

# Methods that have a clean `fl::` equivalent and should be replaced.
FORBIDDEN_METHODS = (
    "print",
    "println",
    "printf",
    "write",
    "read",
    "available",
    "peek",
    "readStringUntil",
    "flush",
    "begin",
)

# Methods that are platform-specific hardware config without an `fl::`
# equivalent. Allow them through.
ALLOWED_METHODS = (
    "setTxBufferSize",
    "setTxTimeoutMs",
)

# Suggested replacement for each forbidden method.
SUGGESTED_REPLACEMENT = {
    "print": "fl::print(...) or fl::cout << ...",
    "println": "fl::println(...) or fl::cout << ... << fl::endl",
    "printf": "fl::cout << ... (build typed values) or fl::println(formatted)",
    "write": "fl::write_bytes(buf, size)",
    "read": "fl::read()",
    "available": "fl::available()",
    "peek": "fl::peek()",
    "readStringUntil": "fl::readLine(delim) (returns fl::optional<fl::string>)",
    "flush": "fl::flush(timeoutMs)",
    "begin": "fl::serial_begin(baudRate)",
}

# Match `Serial.<method>(` — method name captured for replacement hint.
SERIAL_METHOD_PATTERN = re.compile(r"\bSerial\.(\w+)\s*\(")

OPT_OUT_MARKER = "ok serial"


def _enforced_for(file_path: str) -> bool:
    posix = file_path.replace("\\", "/")
    return any(prefix in posix for prefix in ENFORCED_PATH_PREFIXES)


class ExampleSerialChecker(FileContentChecker):
    """Forbid raw Serial.* method calls in enforced example sketches."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino")):
            return False
        if not _enforced_for(file_path):
            return False
        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        violations: list[tuple[int, str]] = []
        in_multiline_comment = False

        for line_number, line in enumerate(file_content.lines, 1):
            stripped = line.strip()

            # Track multi-line comment state.
            if "/*" in line:
                in_multiline_comment = True
            if "*/" in line:
                in_multiline_comment = False
                continue

            if in_multiline_comment:
                continue

            if stripped.startswith("//"):
                continue

            code_part = line.split("//")[0]
            comment_part = line[len(code_part) :].lower()

            # Fast pre-check.
            if "Serial." not in code_part:
                continue

            match = SERIAL_METHOD_PATTERN.search(code_part)
            if not match:
                continue

            method = match.group(1)

            if method in ALLOWED_METHODS:
                continue

            if method not in FORBIDDEN_METHODS:
                # Some other Serial.<method>(...) — leave alone for now.
                continue

            if OPT_OUT_MARKER in comment_part:
                continue

            replacement = SUGGESTED_REPLACEMENT.get(method, "an fl:: variant")
            violations.append(
                (
                    line_number,
                    f"Avoid `Serial.{method}(...)` in enforced examples — "
                    f"use `{replacement}` instead.\n"
                    f"      Rationale: fl:: wrappers carry the non-blocking "
                    f"HWCDC fixes from FastLED #2669 (setTxTimeoutMs=0, "
                    f"guarded flush, host-presence skip). Raw `Serial.{method}` "
                    f"bypasses them.\n"
                    f"      Line: {stripped}\n"
                    f"      If this call is genuinely required (platform-"
                    f"specific config with no fl:: equivalent), suppress with "
                    f"`// ok serial - <reason>` on the same line.",
                )
            )

        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    """Run example-Serial checker standalone."""
    if len(sys.argv) > 1:
        file_path = Path(sys.argv[1]).resolve()
        if not file_path.exists():
            print(f"Error: File not found: {file_path}")
            sys.exit(1)

        checker = ExampleSerialChecker()
        processor = MultiCheckerFileProcessor()
        processor.process_files_with_checkers([str(file_path)], [checker])

        if checker.violations:
            print(
                f"❌ Found raw Serial.* calls in enforced examples "
                f"({len(checker.violations)} file(s)):\n"
            )
            for file_path_str, violation_list in checker.violations.items():
                rel_path = Path(file_path_str).relative_to(PROJECT_ROOT)
                print(f"{rel_path}:")
                for line_num, message in violation_list:
                    print(f"  Line {line_num}: {message}")
            sys.exit(1)
        else:
            print("✅ No raw Serial.* calls in enforced examples.")
            sys.exit(0)
    else:
        checker = ExampleSerialChecker()
        run_checker_standalone(
            checker,
            [str(EXAMPLES_ROOT)],
            "Found raw Serial.* calls in enforced examples",
            extensions=[".cpp", ".h", ".hpp", ".ino"],
        )


if __name__ == "__main__":
    main()
