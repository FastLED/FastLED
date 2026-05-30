#!/usr/bin/env python3
"""Checker for `esp_rom_printf` usage in C/C++ files.

`esp_rom_printf` is a low-level ROM-resident printf intended for very early
boot diagnostics on ESP32-family chips (before the normal logging
infrastructure is up). It writes synchronously to the USB-Serial-JTAG
console and is convenient for boot-time bring-up — but using it from
regular library code is undesirable for several reasons:

  1. It bypasses FastLED's logging / async-logging facilities (FL_LOG_*,
     FL_PRINT, FL_WARN), so its output cannot be filtered, redirected,
     captured, or silenced by users.
  2. It's blocking on a slow peripheral, making it expensive and unsafe
     in hot paths or ISR contexts where logging is normally avoided.
  3. The convention in this codebase is to keep platform-specific I/O
     gated behind a small number of well-known shim points (the
     `usb_serial_jtag_esp32_idf` driver, the `io_esp_jtag_or_uart` shim)
     so it doesn't proliferate into unrelated files.

Banned usage:
  - `esp_rom_printf(...)` anywhere — call FL_LOG_INFO / FL_WARN / FL_PRINT
    (or write to Serial / fl::io) instead.

Opt-out:
  - Append `// ok esp_rom_printf - <reason>` on the same line for genuinely
    legitimate cases (e.g. the USB-Serial-JTAG driver bootstrap itself,
    before any logging is initialized).
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# Pattern to match `esp_rom_printf(...)` calls (function-style, not the name in a comment).
ESP_ROM_PRINTF_PATTERN = re.compile(r"\besp_rom_printf\s*\(")

# Opt-out marker on the same line. Lower-cased before match.
OPT_OUT_MARKER = "ok esp_rom_printf"


class EspRomPrintfChecker(FileContentChecker):
    """Reject raw `esp_rom_printf` calls — use FastLED logging instead."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino", ".cpp.hpp")):
            return False

        # Skip third-party code (defines its own printf paths).
        if "/third_party/" in file_path.replace("\\", "/"):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for raw esp_rom_printf usage."""
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

            # Fast check before regex.
            if "esp_rom_printf" not in code_part:
                continue

            if not ESP_ROM_PRINTF_PATTERN.search(code_part):
                continue

            # Allow opt-out: `// ok esp_rom_printf - <reason>` on the same line.
            if OPT_OUT_MARKER in comment_part:
                continue

            violations.append(
                (
                    line_number,
                    f"Avoid `esp_rom_printf` — use FastLED logging (FL_LOG_INFO, "
                    f"FL_WARN, FL_PRINT) or fl::io instead: {stripped}\n"
                    f"      Rationale: esp_rom_printf bypasses FastLED's logging "
                    f"facilities, blocks on a slow peripheral, and proliferates "
                    f"platform-specific I/O into general code.\n"
                    f"      If this call is genuinely required (e.g. early "
                    f"bootstrap before logging is up), suppress with "
                    f"`// ok esp_rom_printf - <reason>` on the same line.",
                )
            )

        if violations:
            self.violations[file_content.path] = violations

        return []


def main() -> None:
    """Run esp_rom_printf checker standalone."""
    import sys
    from pathlib import Path

    from ci.util.check_files import (
        MultiCheckerFileProcessor,
        run_checker_standalone,
    )

    if len(sys.argv) > 1:
        file_path = Path(sys.argv[1]).resolve()
        if not file_path.exists():
            print(f"Error: File not found: {file_path}")
            sys.exit(1)

        checker = EspRomPrintfChecker()
        processor = MultiCheckerFileProcessor()
        processor.process_files_with_checkers([str(file_path)], [checker])

        if hasattr(checker, "violations") and checker.violations:
            violations = checker.violations
            print(
                f"❌ Found raw esp_rom_printf usage — use FastLED logging instead "
                f"({len(violations)} file(s)):\n"
            )
            for file_path_str, violation_list in violations.items():
                rel_path = Path(file_path_str).relative_to(PROJECT_ROOT)
                print(f"{rel_path}:")
                for line_num, message in violation_list:
                    print(f"  Line {line_num}: {message}")
            sys.exit(1)
        else:
            print("✅ No raw esp_rom_printf usage found.")
            sys.exit(0)
    else:
        checker = EspRomPrintfChecker()
        run_checker_standalone(
            checker,
            [
                str(PROJECT_ROOT / "src"),
                str(PROJECT_ROOT / "examples"),
                str(PROJECT_ROOT / "tests"),
            ],
            "Found raw esp_rom_printf usage — use FastLED logging instead",
            extensions=[".cpp", ".h", ".hpp", ".ino", ".cpp.hpp"],
        )


if __name__ == "__main__":
    main()
