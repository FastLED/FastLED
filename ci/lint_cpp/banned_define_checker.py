#!/usr/bin/env python3
"""Checker for banned #if preprocessor patterns.

Catches wrong #if patterns that should use #ifdef instead:
- #if ESP32 → #ifdef ESP32
- #if defined(FASTLED_RMT5) → #ifdef FASTLED_RMT5
- #if defined(FASTLED_ESP_HAS_CLOCKLESS_SPI) → #ifdef FASTLED_ESP_HAS_CLOCKLESS_SPI
"""

from ci.util.check_files import FileContent, FileContentChecker


WRONG_DEFINES: dict[str, str] = {
    "#if ESP32": "Use #ifdef ESP32 instead of #if ESP32",
    "#if defined(FASTLED_RMT5)": "Use #ifdef FASTLED_RMT5 instead of #if defined(FASTLED_RMT5)",
    "#if defined(FASTLED_ESP_HAS_CLOCKLESS_SPI)": "Use #ifdef FASTLED_ESP_HAS_CLOCKLESS_SPI instead of #if defined(FASTLED_ESP_HAS_CLOCKLESS_SPI)",
}


class BannedDefineChecker(FileContentChecker):
    """Checker for banned #if preprocessor patterns — should use #ifdef instead."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        return file_path.endswith((".cpp", ".h", ".hpp", ".cpp.hpp", ".ino"))

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for banned #if patterns."""
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

            # Skip single-line comments
            if stripped.startswith("//"):
                continue

            # Remove single-line comment portion
            code_part = line.split("//")[0]

            for needle, message in WRONG_DEFINES.items():
                if needle in code_part:
                    violations.append((line_number, message))

        if violations:
            self.violations[file_content.path] = violations

        return []
