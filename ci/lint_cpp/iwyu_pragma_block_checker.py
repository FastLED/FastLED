#!/usr/bin/env python3
"""
Checker to ensure system/platform headers are wrapped in IWYU pragma blocks.

This validates that platform-specific and system headers are wrapped in
// IWYU pragma: begin_keep / end_keep blocks or have inline // IWYU pragma: keep.
Internal FastLED headers (fl/*, fx/*, etc.) should NOT be wrapped.

How it works:
    1. Parse all #include statements
    2. Track IWYU pragma blocks and inline pragmas
    3. Classify headers as:
       - Internal: fl/*, fx/*, sensors/*, lib8tion/* (should be checked by IWYU)
       - Platform: platforms/* (should be wrapped in pragma blocks)
       - System: <...> angle bracket includes (should be wrapped in pragma blocks)
    4. Report violations for platform/system headers not wrapped in pragmas

Examples:
    GOOD:
        // IWYU pragma: begin_keep
        #include <driver/rmt.h>
        #include <esp_system.h>
        // IWYU pragma: end_keep

        #include "fl/pin.h"  // Internal header (checked)

    BAD:
        #include <driver/rmt.h>  // Missing pragma!
        #include "platforms/esp/32/drivers/uart.h"  // Missing pragma!

    ALSO GOOD (inline pragma):
        #include <driver/rmt.h>  // IWYU pragma: keep
        #include "platforms/esp/32/drivers/uart.h"  // IWYU pragma: keep
"""

import re
from pathlib import Path

from ci.util.check_files import CheckerResults, FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# Pattern to match #include statements
INCLUDE_PATTERN = re.compile(
    r'^\s*#include\s+[<"]([^>"]+)[>"](?:\s*//\s*(.*))?'
)  # Captures: (header_path, optional_comment)

# Pattern to match IWYU pragma comments
IWYU_PRAGMA_BEGIN = re.compile(r"^\s*//\s*IWYU\s+pragma:\s*begin_keep", re.IGNORECASE)
IWYU_PRAGMA_END = re.compile(r"^\s*//\s*IWYU\s+pragma:\s*end_keep", re.IGNORECASE)
IWYU_PRAGMA_KEEP_INLINE = re.compile(r"IWYU\s+pragma:\s*keep", re.IGNORECASE)

# Internal FastLED header prefixes (should be checked by IWYU)
INTERNAL_HEADER_PREFIXES = ("fl/", "fx/", "sensors/", "lib8tion/")


class IwyuPragmaBlockChecker(FileContentChecker):
    """Checker that validates system/platform headers are wrapped in IWYU pragma blocks."""

    def __init__(self, all_headers: frozenset[str] | None = None):
        """Initialize the checker.

        Args:
            all_headers: Optional frozenset of all header file paths in the project
                        (used for cross-file validation)
        """
        self.violations = CheckerResults()
        self.all_headers = all_headers or frozenset()

    def should_process_file(self, file_path: str) -> bool:
        """Only process C++ header files (.h, .hpp)."""
        path = Path(file_path)

        # Only check header files
        if path.suffix not in {".h", ".hpp", ".hh", ".hxx"}:
            return False

        # Skip third-party code
        if "/third_party/" in str(path).replace("\\", "/"):
            return False

        # Skip stub platform (test infrastructure)
        if "/platforms/stub/" in str(path).replace("\\", "/"):
            return False

        # Only process files in src/ directory
        try:
            path.relative_to(PROJECT_ROOT / "src")
            return True
        except ValueError:
            return False

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check that system/platform headers are wrapped in IWYU pragma blocks."""
        violations: list[str] = []
        in_pragma_block = False
        line_num = 0

        # Determine if this file is in fl/ directory
        file_path_normalized = str(Path(file_content.path)).replace("\\", "/")
        is_fl_file = "/src/fl/" in file_path_normalized

        for line in file_content.lines:
            line_num += 1

            # Track IWYU pragma block state
            if IWYU_PRAGMA_BEGIN.match(line):
                in_pragma_block = True
                continue
            if IWYU_PRAGMA_END.match(line):
                in_pragma_block = False
                continue

            # Check #include statements
            match = INCLUDE_PATTERN.match(line)
            if not match:
                continue

            header_path = match.group(1)
            comment = match.group(2) or ""

            # Check if inline pragma is present
            has_inline_pragma = bool(IWYU_PRAGMA_KEEP_INLINE.search(comment))

            # Skip if header is wrapped in block or has inline pragma
            if in_pragma_block or has_inline_pragma:
                continue

            # Classify header type
            header_type = self._classify_header(header_path, line)

            # Internal headers should NOT be wrapped (we want IWYU to check them)
            if header_type == "internal":
                continue

            # Give amnesty to top-level platforms/*.h includes (no subdirectories)
            # Example: platforms/is_platform.h, platforms/await.h (AMNESTY)
            # Example: platforms/esp/32/driver.h (NO AMNESTY - requires wrapping)
            if header_type == "platform":
                if self._is_top_level_platform_header(header_path):
                    # Top-level platform headers are always allowed unwrapped
                    continue
                if not is_fl_file:
                    # Platform files can include other platform headers freely
                    continue
                # fl/ files MUST wrap non-top-level platform headers
                violation_msg = self._format_violation(
                    header_path, header_type, line.strip()
                )
                violations.append(violation_msg)
                self.violations.add_violation(
                    file_content.path, line_num, violation_msg
                )

            # System headers MUST always be wrapped (all files)
            if header_type == "system":
                violation_msg = self._format_violation(
                    header_path, header_type, line.strip()
                )
                violations.append(violation_msg)
                self.violations.add_violation(
                    file_content.path, line_num, violation_msg
                )

        return violations

    def _is_top_level_platform_header(self, header_path: str) -> bool:
        """Check if header is a top-level platforms/*.h file (no subdirectories).

        Args:
            header_path: The header path from #include statement

        Returns:
            True if it's platforms/*.h (top-level only), False otherwise

        Examples:
            platforms/is_platform.h → True (amnesty)
            platforms/await.h → True (amnesty)
            platforms/esp/32/driver.h → False (requires wrapping)
        """
        if not header_path.startswith("platforms/"):
            return False

        # Remove "platforms/" prefix
        remainder = header_path[len("platforms/") :]

        # Check if there are any more slashes (subdirectories)
        # Top-level: "is_platform.h" (no slashes)
        # Subdirectory: "esp/32/driver.h" (has slashes)
        return "/" not in remainder

    def _classify_header(self, header_path: str, include_line: str) -> str:
        """Classify header as internal, platform, or system.

        Args:
            header_path: The header path from #include statement
            include_line: The full include line (to check for angle brackets)

        Returns:
            "internal" - FastLED core headers (should be checked by IWYU)
            "platform" - Platform-specific headers (should be wrapped in pragmas)
            "system" - System headers (should be wrapped in pragmas)
        """
        # Check if it's an angle bracket include (system header)
        if "<" in include_line and ">" in include_line:
            return "system"

        # Check if it's an internal FastLED header with directory prefix
        if header_path.startswith(INTERNAL_HEADER_PREFIXES):
            return "internal"

        # Check if it's a root-level FastLED header (src/*.h without directory)
        # Examples: crgb.h, eorder.h, fastled_delay.h, pixeltypes.h
        if "/" not in header_path and header_path.endswith((".h", ".hpp")):
            return "internal"

        # Check if it's a platform header
        if header_path.startswith("platforms/"):
            return "platform"

        # Default to system (conservative - require pragma for unknown headers)
        return "system"

    def _format_violation(
        self, header_path: str, header_type: str, include_line: str
    ) -> str:
        """Format violation message.

        Args:
            header_path: The header path from #include statement
            header_type: The classification (platform/system)
            include_line: The full include line

        Returns:
            Formatted violation message
        """
        type_desc = "platform" if header_type == "platform" else "system"
        return (
            f"{type_desc.capitalize()} header not wrapped in IWYU pragma block: {include_line}\n"
            f"  Wrap in:\n"
            f"    // IWYU pragma: begin_keep\n"
            f"    {include_line}\n"
            f"    // IWYU pragma: end_keep\n"
            f"  Or add inline pragma:\n"
            f"    {include_line}  // IWYU pragma: keep"
        )


def main() -> None:
    """Run IWYU pragma block checker standalone."""
    from ci.util.check_files import collect_files_to_check, run_checker_standalone

    # Collect all header files for cross-validation
    src_root = PROJECT_ROOT / "src"
    all_files = collect_files_to_check([str(src_root)])
    all_headers = frozenset(f for f in all_files if f.endswith((".h", ".hpp")))

    checker = IwyuPragmaBlockChecker(all_headers=all_headers)
    run_checker_standalone(
        checker,
        [str(src_root)],
        "Found headers with system/platform includes not wrapped in IWYU pragmas",
        extensions=[".h", ".hpp"],
    )


if __name__ == "__main__":
    main()
