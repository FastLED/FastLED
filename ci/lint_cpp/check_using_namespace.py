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
from typing import List, Tuple

from ci.util.paths import PROJECT_ROOT


def find_using_namespace_violations(file_path: Path) -> List[Tuple[int, str]]:
    """
    Find 'using namespace' declarations in a file.

    Returns:
        List of (line_number, line_content) tuples for violations
    """
    violations: List[Tuple[int, str]] = []

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


def check_header_files(root_dir: Path, extensions: List[str]) -> bool:
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

    violations_found = check_header_files(args.root, args.extensions)

    if violations_found:
        if not args.quiet:
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
