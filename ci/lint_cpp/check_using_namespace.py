#!/usr/bin/env python3

"""
Check for 'using namespace' declarations in header files.

Using namespace declarations in headers can cause namespace pollution
and unexpected symbol conflicts when headers are included.
"""

import argparse
import re
import sys
from pathlib import Path

from ci.util.check_files import (
    FileContent,
    FileContentChecker,
    MultiCheckerFileProcessor,
    collect_files_to_check,
)
from ci.util.paths import PROJECT_ROOT


class UsingNamespaceChecker(FileContentChecker):
    """FileContentChecker implementation for detecting 'using namespace' in headers."""

    def __init__(self):
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed (only PROJECT_ROOT/src/fl/ header files)."""
        # Fast normalized path check
        normalized_path = file_path.replace("\\", "/")

        # Must be in /src/fl/ subdirectory
        if "/src/fl/" not in normalized_path:
            return False

        # Must NOT be in examples or tests (examples/*/src/fl/ should not match)
        if "/examples/" in normalized_path or "/tests/" in normalized_path:
            return False

        # Only check header files
        return any(file_path.endswith(ext) for ext in [".h", ".hpp", ".hxx", ".hh"])

    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check file content for 'using namespace' declarations."""
        violations: list[tuple[int, str]] = []
        file_content.content.splitlines(keepends=True)

        for line_num, line in enumerate(file_content.lines, 1):
            # Remove comments to avoid false positives
            line_clean = re.sub(r"//.*$", "", line)
            line_clean = re.sub(r"/\*.*?\*/", "", line_clean)

            # Look for 'using namespace' declarations
            # Match patterns like:
            # - using namespace std;
            # - using namespace foo::bar;
            # But not:
            # - using std::vector; (using declaration, not namespace)
            if re.search(r"\busing\s+namespace\s+\w+(?:::\w+)*\s*;", line_clean):
                # Skip macro definitions
                if re.search(r"#define\s+\w+.*using\s+namespace", line_clean):
                    continue

                # Skip conditional using statements that are part of API design
                # Look at previous few lines for #if statements
                is_conditional = False
                for i in range(max(0, line_num - 6), line_num - 1):
                    if i < len(file_content.lines):
                        prev_line = file_content.lines[i].strip()
                        if re.search(r"#if\s+.*FORCE_USE_NAMESPACE", prev_line):
                            is_conditional = True
                            break

                if not is_conditional:
                    violations.append((line_num, line.strip()))

        if violations:
            self.violations[file_content.path] = violations

        return []  # We collect violations internally


def find_using_namespace_violations(file_path: Path) -> list[tuple[int, str]]:
    """
    Find 'using namespace' declarations in a file.

    Returns:
        List of (line_number, line_content) tuples for violations
    """
    violations: list[tuple[int, str]] = []

    try:
        with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()

            for line_num, line in enumerate(lines, 1):
                # Remove comments to avoid false positives
                line_clean = re.sub(r"//.*$", "", line)
                line_clean = re.sub(r"/\*.*?\*/", "", line_clean)

                # Look for 'using namespace' declarations
                # Match patterns like:
                # - using namespace std;
                # - using namespace foo::bar;
                # But not:
                # - using std::vector; (using declaration, not namespace)
                if re.search(r"\busing\s+namespace\s+\w+(?:::\w+)*\s*;", line_clean):
                    # Skip macro definitions
                    if re.search(r"#define\s+\w+.*using\s+namespace", line_clean):
                        continue

                    # Skip conditional using statements that are part of API design
                    # Look at previous few lines for #if statements
                    is_conditional = False
                    for i in range(max(0, line_num - 5), line_num):
                        if i < len(lines):
                            prev_line = lines[i].strip()
                            if re.search(r"#if\s+.*FORCE_USE_NAMESPACE", prev_line):
                                is_conditional = True
                                break

                    if not is_conditional:
                        violations.append((line_num, line.strip()))

    except (UnicodeDecodeError, PermissionError) as e:
        print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)

    return violations


def check_header_files(root_dir: Path, extensions: list[str]) -> bool:
    """
    Check all header files in the given directory for using namespace violations.

    Returns:
        True if violations found, False otherwise
    """
    violations_found = False

    for ext in extensions:
        for file_path in root_dir.rglob(f"*.{ext}"):
            # Skip third-party directories, build directories, and virtual environments
            if any(
                part in str(file_path).lower()
                for part in [
                    "third_party",
                    "vendor",
                    "external",
                    ".build",
                    ".pio",
                    ".venv",
                    "libdeps",
                ]
            ):
                continue

            violations = find_using_namespace_violations(file_path)

            if violations:
                violations_found = True
                print(f"\n{file_path}:")
                for line_num, line_content in violations:
                    print(f"  Line {line_num}: {line_content}")

    return violations_found


def main():
    parser = argparse.ArgumentParser(
        description="Check for 'using namespace' declarations in header files"
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=PROJECT_ROOT / "src" / "fl",
        help="Root directory to search (default: PROJECT_ROOT/src/fl)",
    )
    parser.add_argument(
        "--extensions",
        nargs="+",
        default=["h", "hpp", "hxx", "hh"],
        help="Header file extensions to check (default: h hpp hxx hh)",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Only print violations, no summary messages",
    )

    args = parser.parse_args()

    if not args.quiet:
        print(f"Checking for 'using namespace' declarations in {args.root}")
        print(f"Extensions: {', '.join(args.extensions)}")
        print()

    # Use the new FileContentChecker-based approach
    root_dir = str(args.root)
    extensions = [f".{ext}" for ext in args.extensions]

    # Collect files using collect_files_to_check
    files_to_check = collect_files_to_check([root_dir], extensions=extensions)

    # Create checker and processor
    checker = UsingNamespaceChecker()
    processor = MultiCheckerFileProcessor()

    # Process all files in a single pass
    processor.process_files_with_checkers(files_to_check, [checker])

    # Get violations from checker
    violations = checker.violations

    if violations:
        if not args.quiet:
            for file_path, line_info in violations.items():
                print(f"\n{file_path}:")
                for line_num, line_content in line_info:
                    print(f"  Line {line_num}: {line_content}")
            print("\n❌ Found 'using namespace' declarations in header files!")
            print("Consider moving these to implementation files (.cpp, .cc) or using")
            print("specific 'using' declarations instead (e.g., 'using std::vector;')")
        sys.exit(1)
    else:
        if not args.quiet:
            print("✅ No 'using namespace' declarations found in header files")
        sys.exit(0)


if __name__ == "__main__":
    main()
