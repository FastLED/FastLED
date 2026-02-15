#!/usr/bin/env python3
"""
Script to find files using FL_HAS_INCLUDE macro and ensure they include fl/has_include.h

This script:
1. Finds all files using FL_HAS_INCLUDE
2. Checks if they include "fl/has_include.h"
3. Reports missing includes
4. Optionally fixes them by adding the include
"""

import _thread
import re
import sys
from pathlib import Path
from typing import Set, Tuple


# Root directory of the project
PROJECT_ROOT = Path(__file__).parent.parent.resolve()

# File patterns to search (C++ files)
FILE_PATTERNS = ["**/*.h", "**/*.hpp", "**/*.cpp", "**/*.ino"]

# Directories to exclude
EXCLUDE_DIRS = {
    ".git",
    ".build",
    ".venv",
    "__pycache__",
    "node_modules",
    ".fbuild",
}

# Files to exclude (these define FL_HAS_INCLUDE itself)
EXCLUDE_FILES = {
    "src/fl/has_include.h",  # This file DEFINES FL_HAS_INCLUDE
}


def should_skip_path(path: Path) -> bool:
    """Check if path should be skipped."""
    # Skip if in excluded directory
    for part in path.parts:
        if part in EXCLUDE_DIRS:
            return True

    # Skip if in excluded files list
    relative = path.relative_to(PROJECT_ROOT)
    if str(relative).replace("\\", "/") in EXCLUDE_FILES:
        return True

    return False


def find_files_using_macro(macro_name: str) -> set[Path]:
    """Find all files that use the specified macro."""
    files_with_macro: set[Path] = set()

    for pattern in FILE_PATTERNS:
        for file_path in PROJECT_ROOT.glob(pattern):
            if not file_path.is_file() or should_skip_path(file_path):
                continue

            try:
                content = file_path.read_text(encoding="utf-8", errors="ignore")
                # Match FL_HAS_INCLUDE(...) usage (not definitions)
                # Look for FL_HAS_INCLUDE followed by (
                if re.search(rf"\b{macro_name}\s*\(", content):
                    # Exclude lines that define the macro
                    lines_with_macro = [
                        line
                        for line in content.splitlines()
                        if re.search(rf"\b{macro_name}\s*\(", line)
                    ]

                    # Check if any line is NOT a definition
                    has_usage = False
                    for line in lines_with_macro:
                        # Skip #define FL_HAS_INCLUDE lines
                        if re.match(r"^\s*#\s*define\s+" + macro_name, line):
                            continue
                        # Skip comments
                        if "//" in line and line.index("//") < line.index(macro_name):
                            continue
                        has_usage = True
                        break

                    if has_usage:
                        files_with_macro.add(file_path)
            except KeyboardInterrupt:
                _thread.interrupt_main()
                raise
            except Exception as e:
                print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)

    return files_with_macro


def check_has_include(file_path: Path, include_path: str) -> bool:
    """Check if file includes the specified header."""
    try:
        content = file_path.read_text(encoding="utf-8", errors="ignore")
        # Match #include "fl/has_include.h" or #include <fl/has_include.h>
        pattern = rf'^\s*#\s*include\s+[<"].*{re.escape(include_path)}[">]'
        return bool(re.search(pattern, content, re.MULTILINE))
    except KeyboardInterrupt:
        _thread.interrupt_main()
        raise
    except Exception as e:
        print(f"Warning: Could not read {file_path}: {e}", file=sys.stderr)
        return False


def find_insertion_point(content: str) -> tuple[int, str]:
    """
    Find the best insertion point for the include.

    Returns (line_number, indent_string)
    """
    lines = content.splitlines(keepends=True)

    # Strategy 1: After header guard (preferred)
    for i, line in enumerate(lines):
        if re.match(r"^\s*#\s*define\s+\w+_H\b", line):
            # Found header guard define, insert after it
            # Skip any blank lines after the guard
            j = i + 1
            while j < len(lines) and lines[j].strip() == "":
                j += 1

            # If there's already an include, match its indentation
            if j < len(lines) and "#include" in lines[j]:
                match = re.match(r"^(\s*)", lines[j])
                indent = match.group(1) if match else ""
                return j, indent

            # Otherwise use no indentation
            return j, ""

    # Strategy 2: Before first #include
    for i, line in enumerate(lines):
        if re.match(r"^\s*#\s*include\s+", line):
            match = re.match(r"^(\s*)", line)
            indent = match.group(1) if match else ""
            return i, indent

    # Strategy 3: After #define CRASH_HANDLER_H or similar
    for i, line in enumerate(lines):
        if re.match(r"^\s*#\s*define\s+", line):
            return i + 1, ""

    # Strategy 4: After #ifndef
    for i, line in enumerate(lines):
        if re.match(r"^\s*#\s*ifndef\s+", line):
            return i + 1, ""

    # Fallback: beginning of file
    return 0, ""


def add_include(file_path: Path, include_path: str, dry_run: bool = True) -> bool:
    """
    Add the include to the file.

    Returns True if file was modified (or would be modified in dry-run).
    """
    try:
        content = file_path.read_text(encoding="utf-8", errors="ignore")

        # Find insertion point
        line_num, indent = find_insertion_point(content)

        # Build the include line
        include_line = f'{indent}#include "{include_path}"  // IWYU pragma: keep\n'

        # Insert the include
        lines = content.splitlines(keepends=True)

        # If inserting at line_num, we want to add BEFORE that line
        # But we want to add after header guard and before other includes
        # So we need to add a blank line after the include if there isn't one

        if line_num < len(lines):
            # Insert before line_num
            new_lines = lines[:line_num] + [include_line, "\n"] + lines[line_num:]
        else:
            # Append at end
            new_lines = lines + ["\n", include_line, "\n"]

        new_content = "".join(new_lines)

        if dry_run:
            print(f"  Would add include at line {line_num + 1}")
            return True
        else:
            file_path.write_text(new_content, encoding="utf-8")
            print(f"  ✓ Added include at line {line_num + 1}")
            return True

    except KeyboardInterrupt:
        _thread.interrupt_main()
        raise
    except Exception as e:
        print(f"  ✗ Error modifying file: {e}", file=sys.stderr)
        return False


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="Find and fix files using FL_HAS_INCLUDE without including fl/has_include.h"
    )
    parser.add_argument(
        "--fix",
        action="store_true",
        help="Automatically add missing includes (default: dry-run mode)",
    )
    parser.add_argument(
        "--macro",
        default="FL_HAS_INCLUDE",
        help="Macro to search for (default: FL_HAS_INCLUDE)",
    )
    parser.add_argument(
        "--include",
        default="fl/has_include.h",
        help="Include path to check/add (default: fl/has_include.h)",
    )

    args = parser.parse_args()

    print(f"🔍 Searching for files using {args.macro}...")
    files_with_macro = find_files_using_macro(args.macro)
    print(f"   Found {len(files_with_macro)} files using {args.macro}")

    print(f"\n🔍 Checking which files include {args.include}...")
    files_with_include = {
        f for f in files_with_macro if check_has_include(f, args.include)
    }
    print(f"   {len(files_with_include)} files already include {args.include}")

    print(f"\n📊 Finding files missing the include...")
    files_missing_include = files_with_macro - files_with_include

    if not files_missing_include:
        print(f"   ✅ All files using {args.macro} include {args.include}!")
        return 0

    print(f"   ⚠️  {len(files_missing_include)} files are missing the include:\n")

    for file_path in sorted(files_missing_include):
        relative = file_path.relative_to(PROJECT_ROOT)
        print(f"   • {relative}")

    if args.fix:
        print(f"\n🔧 Adding missing includes...")
        fixed_count = 0
        for file_path in sorted(files_missing_include):
            relative = file_path.relative_to(PROJECT_ROOT)
            print(f"\n   {relative}:")
            if add_include(file_path, args.include, dry_run=False):
                fixed_count += 1

        print(f"\n✅ Fixed {fixed_count}/{len(files_missing_include)} files")
        return 0
    else:
        print(f"\n💡 Run with --fix to automatically add the missing includes")
        print(f"   Example: uv run python ci/fix_has_include.py --fix")
        return 1


if __name__ == "__main__":
    sys.exit(main())
