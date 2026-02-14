#!/usr/bin/env python3
"""
Check that source files only include platform trampoline headers,
not deep platform-specific implementation headers.

Architecture rule:
- ✅ ALLOWED: #include "platforms/init.h" (top-level trampolines)
- ❌ BLOCKED: #include "platforms/shared/int_windows.h" (deep platform includes)

Scope:
- Restricts: src/fl/** and root src/*.{cpp,h} files
- Allows: src/platforms/** (can include anything)

Rationale:
- Platform trampolines (platforms/*.h) provide platform abstraction
- Deep platform headers (platforms/*/*) are internal implementation details
- Only platform implementation code (src/platforms/**) should include them
"""

import re

from ci.util.check_files import FileContent, FileContentChecker


# Pattern to match #include "platforms/X/Y/..." (2+ levels deep)
DEEP_PLATFORM_INCLUDE = re.compile(
    r'^\s*#\s*include\s+[<"]platforms/([^/"]+)/([^"]+)[">]'
)

# Suggested replacements for common violations
REPLACEMENTS = {
    "platforms/shared/int_windows.h": "platforms/int.h",
    "platforms/shared/int.h": "platforms/int.h",
    "platforms/esp/": "platforms/init.h",
    "platforms/arm/": "platforms/init.h",
    "platforms/stub/": "platforms/init.h",
    "platforms/avr/": "platforms/init.h",
    "platforms/wasm/": "platforms/init.h",
}


def get_suggestion(include_path: str) -> str:
    """Get suggested replacement for a deep platform include."""
    for pattern, replacement in REPLACEMENTS.items():
        if pattern in include_path:
            return replacement
    return "platforms/*.h (appropriate trampoline)"


class PlatformTrampolineChecker(FileContentChecker):
    """Checker for ensuring only platform trampolines are used (not deep platform headers)."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed.

        Process:
        - src/fl/** files
        - Root src/*.cpp and src/*.h files

        Skip:
        - src/platforms/** files (can include anything)
        """
        # Normalize path
        normalized_path = file_path.replace("\\", "/")

        # Skip if not in src/
        if "/src/" not in normalized_path:
            return False

        # Extract parts after src/
        parts_after_src = normalized_path.split("/src/", 1)[1].split("/")

        # Allow src/platforms/** to include anything
        if parts_after_src[0] == "platforms":
            return False

        # Check src/fl/**
        if parts_after_src[0] == "fl":
            return True

        # Check root src/*.{cpp,h} (direct children of src/)
        if len(parts_after_src) == 1 and file_path.endswith((".cpp", ".h", ".hpp")):
            return True

        return False

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file for deep platform includes."""
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

            # Check for deep platform includes
            match = DEEP_PLATFORM_INCLUDE.search(code_part)
            if match and not has_exception:
                include_path = f"platforms/{match.group(1)}/{match.group(2)}"
                suggestion = get_suggestion(include_path)

                violations.append(
                    (
                        line_number,
                        f'Forbidden deep platform header: #include "{include_path}" - '
                        f'Use "{suggestion}" instead (platform trampolines only). '
                        f"Deep includes bypass the trampoline architecture. "
                        f'If necessary, add "// ok platform headers" comment to suppress.',
                    )
                )

        # Store violations if any found
        if violations:
            self.violations[file_content.path] = violations

        return []  # MUST return empty list


def main() -> None:
    """Run platform trampoline checker standalone."""
    from ci.util.check_files import run_checker_standalone  # type: ignore[attr-defined]
    from ci.util.paths import PROJECT_ROOT

    checker = PlatformTrampolineChecker()
    run_checker_standalone(
        checker,
        [
            str(PROJECT_ROOT / "src" / "fl"),
            str(PROJECT_ROOT / "src"),
        ],
        "Found deep platform header violations",
        extensions=[".cpp", ".h", ".hpp"],
    )


if __name__ == "__main__":
    main()
