#!/usr/bin/env python3
"""
cpp_lint.py - FastLED C++ Linter

Performs multiple checks on C++ source files:
1. Checks for relative includes (those containing "..") which can cause build issues
2. Ensures internal headers use 'fl/fastled.h' instead of 'FastLED.h'
"""

import argparse
import re
import sys
from pathlib import Path
from typing import List, Tuple, Set


def find_cpp_files(root_dir: Path, exclude_dirs: Set[str]) -> List[Path]:
    """Find all C++ source and header files, excluding specified directories."""
    cpp_extensions = {'.cpp', '.h', '.hpp', '.cc', '.cxx', '.ino'}
    cpp_files: List[Path] = []

    for ext in cpp_extensions:
        for file_path in root_dir.rglob(f'*{ext}'):
            # Check if any parent directory should be excluded
            if any(excluded in file_path.parts for excluded in exclude_dirs):
                continue
            cpp_files.append(file_path)

    return cpp_files


def check_relative_includes(file_path: Path) -> List[Tuple[int, str]]:
    """
    Check a file for #include directives containing ".."

    Returns:
        List of (line_number, line_content) tuples for offending includes
    """
    violations: List[Tuple[int, str]] = []

    # Regex to match #include directives with ".." in the path
    # Matches both #include "path/with/../file.h" and #include <path/with/../file.h>
    include_pattern = re.compile(r'^\s*#\s*include\s+[<"]([^>"]*\.\..*)[>"]')

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line_num, line in enumerate(f, start=1):
                match = include_pattern.match(line)
                if match:
                    violations.append((line_num, line.rstrip()))
    except Exception as e:
        print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)

    return violations


def check_fastled_header_usage(file_path: Path) -> List[Tuple[int, str]]:
    """
    Check header files for #include "FastLED.h" - internal headers should use fl/fastled.h

    Returns:
        List of (line_number, line_content) tuples for offending includes
    """
    violations: List[Tuple[int, str]] = []

    # Only check header files
    if file_path.suffix not in {'.h', '.hpp'}:
        return violations

    # Skip FastLED.h itself and fastspi.h (they're public headers)
    if file_path.name in {'FastLED.h', 'fastspi.h'}:
        return violations

    # Skip example files
    if 'examples' in file_path.parts:
        return violations

    # Only check files in src/ directory
    if 'src' not in file_path.parts:
        return violations

    # Regex to match #include "FastLED.h" or #include <FastLED.h>
    # This should match the root header, not fl/fastled.h
    include_pattern = re.compile(r'^\s*#\s*include\s+[<"]FastLED\.h[>"]')

    # Allow exemptions via "// ok include" comment on the same line
    exemption_pattern = re.compile(r'//\s*ok\s+include', re.IGNORECASE)

    # Pattern to detect #define FASTLED_INTERNAL
    fastled_internal_pattern = re.compile(r'^\s*#\s*define\s+FASTLED_INTERNAL')

    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()

        for line_num, line in enumerate(lines, start=1):
            match = include_pattern.match(line)
            if match:
                # Check if the line has an exemption comment
                if exemption_pattern.search(line):
                    continue

                # Check if FASTLED_INTERNAL was defined within the previous 5 lines
                # This allows for legitimate uses with #define FASTLED_INTERNAL before #include
                has_internal_define = False
                lookback = min(5, line_num - 1)
                for i in range(1, lookback + 1):
                    prev_line = lines[line_num - i - 1]
                    if fastled_internal_pattern.match(prev_line):
                        has_internal_define = True
                        break

                if not has_internal_define:
                    violations.append((line_num, line.rstrip()))
    except Exception as e:
        print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)

    return violations


def main() -> int:
    """Main entry point for the linter."""
    parser = argparse.ArgumentParser(
        description='Check C++ files for common issues (relative includes, FastLED.h usage)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  uv run cpp_lint.py              # Check all C++ files in project
  uv run cpp_lint.py src/         # Check only src/ directory
  uv run cpp_lint.py --exclude .pio # Exclude additional directories
        """
    )
    parser.add_argument(
        'paths',
        nargs='*',
        default=['.'],
        help='Paths to check (default: current directory)'
    )
    parser.add_argument(
        '--exclude',
        action='append',
        default=[],
        help='Additional directories to exclude (can be specified multiple times)'
    )

    args = parser.parse_args()

    # Default exclusions
    exclude_dirs: Set[str] = {
        '.git',
        '.pio',
        'build',
        'node_modules',
        '__pycache__',
        '.venv',
        'venv',
    }
    exclude_dirs.update(args.exclude)

    # Collect all files to check
    all_files: List[Path] = []
    for path_str in args.paths:
        path = Path(path_str)
        if not path.exists():
            print(f"Error: Path does not exist: {path}", file=sys.stderr)
            return 1

        if path.is_file():
            all_files.append(path)
        else:
            all_files.extend(find_cpp_files(path, exclude_dirs))

    if not all_files:
        print("No C++ files found to check.", file=sys.stderr)
        return 0

    # Check all files
    total_violations = 0
    files_with_violations = 0

    for file_path in sorted(all_files):
        # Check for relative includes
        violations = check_relative_includes(file_path)
        if violations:
            files_with_violations += 1
            total_violations += len(violations)

            print(f"\n{file_path}:")
            for line_num, line in violations:
                print(f"  {line_num}: {line}")
                print(f"      ^ Error: Relative include path contains '..'")

        # Check for FastLED.h usage in internal headers
        header_violations = check_fastled_header_usage(file_path)
        if header_violations:
            if not violations:  # Don't double-count files
                files_with_violations += 1
            total_violations += len(header_violations)

            if not violations:  # Only print filename if not already printed
                print(f"\n{file_path}:")
            for line_num, line in header_violations:
                print(f"  {line_num}: {line}")
                print(f"      ^ Error: Internal header should use 'fl/fastled.h' instead of 'FastLED.h'")
                print(f"      ^ Add '// ok include' comment to exempt this line if necessary")

    # Summary
    print(f"\nChecked {len(all_files)} files")
    if total_violations > 0:
        print(f"Found {total_violations} violation(s) in {files_with_violations} file(s)")
        print("\nRelative includes (paths with '..') should be avoided.")
        print("Use absolute includes from project root or proper include paths instead.")
        print("\nInternal headers should use 'fl/fastled.h' instead of 'FastLED.h'.")
        print("The 'FastLED.h' header is for end-user sketches only.")
        return 1
    else:
        print("No violations found (ok)")
        return 0


if __name__ == '__main__':
    sys.exit(main())
