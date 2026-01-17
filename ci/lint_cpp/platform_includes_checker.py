#!/usr/bin/env python3
"""
Lint check: Prevent deep platform-specific header includes in platform-agnostic code.

FastLED uses a platform trampoline architecture where platform-agnostic code should
use trampoline headers (like fl/int.h) instead of directly including deep platform-specific
headers (like platforms/shared/int_windows.h or platforms/esp/32/some_driver.h).

Architecture:
    User Code → fl/int.h → platforms/int.h → platforms/{platform}/int*.h

Rules:
    1. Code in src/fl/** MUST NOT include deep platform headers (more than one layer deep)
    2. ALLOWED: #include "platforms/*.h" (top-level platform headers only)
    3. FORBIDDEN: #include "platforms/{platform}/**/*.h" (deep platform headers)
    4. EXCEPTION: Add "// ok platform headers" comment to bypass the check

Example violations:
    // ❌ FAIL: Deep platform-specific includes
    #include "platforms/shared/int_windows.h"
    #include "platforms/esp/32/some_driver.h"
    #include "platforms/wasm/js/interface.h"

Allowed includes:
    // ✅ PASS: Top-level platform headers
    #include "platforms/int.h"
    #include "platforms/io.h"

    // ✅ PASS: fl/ headers (use these in fl/ code)
    #include "fl/int.h"
    #include "fl/io.h"

Exception mechanism:
    // ✅ PASS: Exception comment allows deep includes
    #include "platforms/esp/32/some_driver.h"  // ok platform headers

For more details, see:
    - src/fl/int.h (platform-agnostic interface)
    - src/platforms/README.md (platform architecture documentation)
    - src/platforms/int.h (platform dispatcher)
"""

# pyright: reportUnknownMemberType=false
import re

from ci.util.check_files import EXCLUDED_FILES, FileContent, FileContentChecker


# Pattern to match deep platform header includes (more than one layer deep)
# Examples:
#   - platforms/shared/int_windows.h (DEEP - has subdirectory)
#   - platforms/esp/32/driver.h (DEEP - has subdirectory)
#   - platforms/int.h (ALLOWED - top-level only)
DEEP_PLATFORM_HEADER_PATTERN = re.compile(r'#include\s+"platforms/[^/]+/[^"]+\.h[^"]*"')


class PlatformIncludesChecker(FileContentChecker):
    """Checker for direct platform-specific header includes."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Only process HEADER files in platform-agnostic locations.

        This checker focuses on headers (.h, .hpp) because they define public APIs
        that should remain platform-agnostic. Implementation files (.cpp) are allowed
        to include platform-specific headers when necessary for implementation.
        """
        # Normalize path separators for cross-platform compatibility
        normalized_path = file_path.replace("\\", "/")

        # Check file extension - ONLY check headers (.h, .hpp), not .cpp or .ino
        if not file_path.endswith((".h", ".hpp")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        # ALLOWED: Files in platform implementation directories (can use platform headers)
        if "/platforms/" in normalized_path:
            # Allow platform implementation code to use platform headers
            # Exception: Don't check files in src/platforms/*/
            return False

        # ALLOWED: Platform-specific test files
        if "/tests/platforms/" in normalized_path:
            return False

        # CHECK: Platform-agnostic HEADER locations must use trampolines
        platform_agnostic_locations = [
            "/src/fl/",
            "/src/fx/",
            "/src/lib8tion/",
            "/src/sensors/",
            "/tests/fl/",
            "/tests/fx/",
            "/tests/ftl/",
            "/examples/",
        ]

        return any(
            location in normalized_path for location in platform_agnostic_locations
        )

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file for deep platform-specific header includes."""
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

            # Skip if in multi-line comment
            if in_multiline_comment:
                continue

            # Skip single-line comments
            if stripped.startswith("//"):
                continue

            # Split line to separate code from inline comments
            code_part = line.split("//")[0]
            comment_part = line.split("//", 1)[1] if "//" in line else ""

            # Check if exception comment is present
            has_exception = (
                "ok platform headers" in comment_part.lower()
                or "ok -- platform headers" in comment_part.lower()
            )

            # Check for deep platform header includes (more than one layer)
            match = DEEP_PLATFORM_HEADER_PATTERN.search(code_part)
            if match and not has_exception:
                include_statement = match.group(0).strip()

                # Extract the header path from the include statement
                header_match = re.search(r'#include\s+"([^"]+)"', include_statement)
                if header_match:
                    header_path = header_match.group(1)

                    violations.append(
                        (
                            line_number,
                            f"Forbidden deep platform header: {include_statement} - "
                            f"Code in fl/** must use top-level platform headers (platforms/*.h) "
                            f"or fl/ alternatives instead of deep platform-specific headers. "
                            f"Deep includes (platforms/{{platform}}/**/*.h) bypass the trampoline architecture. "
                            f"If this include is necessary, add '// ok platform headers' comment to suppress. "
                            f"See src/platforms/README.md for architecture details.",
                        )
                    )

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run platform includes checker standalone."""
    from ci.util.check_files import run_checker_standalone
    from ci.util.paths import PROJECT_ROOT

    checker = PlatformIncludesChecker()
    run_checker_standalone(
        checker,
        [
            str(PROJECT_ROOT / "src" / "fl"),
            str(PROJECT_ROOT / "src" / "fx"),
            str(PROJECT_ROOT / "tests" / "fl"),
            str(PROJECT_ROOT / "tests" / "fx"),
            str(PROJECT_ROOT / "examples"),
        ],
        "Found direct platform-specific header includes",
        extensions=[".cpp", ".h", ".hpp", ".ino"],
    )


if __name__ == "__main__":
    main()
