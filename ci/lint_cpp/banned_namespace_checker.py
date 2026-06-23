#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false
"""Checker for detecting banned namespace patterns.

This linter detects the use of banned namespace patterns like 'fl::fl',
which indicates that a file was incorrectly included inside a namespace declaration.
"""

import re

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


# List of banned namespace patterns
BANNED_NAMESPACES = [
    "fl::fl",  # Indicates improper include inside namespace
]

# Pre-compiled regex patterns for each banned namespace (avoid re-compiling per file)
_BANNED_NS_PATTERNS: list[re.Pattern[str]] = [
    re.compile(rf"\b{re.escape(ns)}\b") for ns in BANNED_NAMESPACES
]


class BannedNamespaceChecker(FileContentChecker):
    """Checker for detecting banned namespace patterns."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed."""
        # Skip build directories and third-party code
        skip_patterns = [".build", ".pio", ".venv", "third_party", "vendor"]
        if any(pattern in file_path for pattern in skip_patterns):
            return False

        # Check C++ source and header files
        return file_path.endswith((".cpp", ".h", ".hpp", ".cc"))

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for banned namespace patterns."""
        # Fast file-level check: skip if no banned namespace keyword in file
        if not any(ns in file_content.content for ns in BANNED_NAMESPACES):
            return []

        violations: list[tuple[int, str]] = []

        for line_num, line in enumerate(file_content.lines, 1):
            # Skip empty lines and comments
            stripped_line = line.strip()
            if not stripped_line or stripped_line.startswith("//"):
                continue

            # Remove inline comments for more accurate checking
            code_part = line.split("//")[0]

            # Fast first pass: skip regex if no banned namespace keyword in line
            if not any(ns in code_part for ns in BANNED_NAMESPACES):
                continue

            # Check against all banned namespace patterns (pre-compiled at module level)
            for pattern in _BANNED_NS_PATTERNS:
                if pattern.search(code_part):
                    violations.append((line_num, line.strip()))
                    break  # Only report once per line

        if violations:
            self.violations[file_content.path] = violations

        return []  # We collect violations internally


def main() -> None:
    """Run banned namespace checker standalone."""
    import sys

    from ci.util.check_files import MultiCheckerFileProcessor, collect_files_to_check

    checker = BannedNamespaceChecker()

    # Collect files to check
    files_to_check = collect_files_to_check(
        [str(PROJECT_ROOT / "src"), str(PROJECT_ROOT / "tests")],
        extensions=[".cpp", ".h", ".hpp", ".cc"],
    )

    # Run checker
    processor = MultiCheckerFileProcessor()
    processor.process_files_with_checkers(files_to_check, [checker])

    # Check for violations
    violations = checker.violations

    if violations:
        # Print violations with custom error message
        print("❌ Improper include inside namespace detected!")
        print("   If you are using 'fl::fl', this means you included a file inside")
        print("   the namespace declaration. Please review what you did and fix it.")
        print()
        print("   Common causes:")
        print("   - #include directive placed after 'namespace fl {' declaration")
        print("   - File included inside namespace scope instead of at global scope")
        print()
        print(f"Found {sum(len(v) for v in violations.values())} violation(s):")
        print()

        for file_path, file_violations in sorted(violations.items()):
            rel_path = file_path.replace(str(PROJECT_ROOT), "").lstrip("\\/")
            print(f"{rel_path}:")
            for line_num, line_content in file_violations:
                print(f"  Line {line_num}: {line_content}")
            print()

        sys.exit(1)
    else:
        print("✅ No banned namespace patterns found.")
        sys.exit(0)


if __name__ == "__main__":
    main()
