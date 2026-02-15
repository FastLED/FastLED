#!/usr/bin/env python3
"""
IWYU Pragma Linter for Platform Headers

Ensures platform implementation headers have explicit IWYU pragma markings.
- Files in platforms/**/ (subdirectories) should be marked private
- Files in platforms/ (root) are public by default (no pragma required)

Note: IWYU only supports 'private', 'keep', 'export', 'no_include' pragmas.
The 'public' pragma does not exist - headers are public by default.
"""

import argparse
import sys
from pathlib import Path


def has_iwyu_pragma(file: Path) -> bool:
    """Check if file has IWYU pragma"""
    try:
        content = file.read_text(encoding="utf-8")
        return "IWYU pragma:" in content
    except UnicodeDecodeError:
        # Skip binary files
        return True
    except OSError as e:
        print(f"  ⚠️  Error reading {file}: {e}", file=sys.stderr)
        return True  # Skip files we can't read


def is_subdirectory_file(file: Path, platforms_dir: Path) -> bool:
    """Check if file is in a subdirectory (not directly in platforms/)"""
    return file.parent != platforms_dir


def check_single_file(file_path: str, project_root: Path) -> int:
    """Check a single file for IWYU pragma. Returns 0 if OK, 1 if missing pragma."""
    file = Path(file_path)

    if not file.exists():
        print(f"❌ File not found: {file_path}")
        return 1

    # Only check files in platforms/ directory
    platforms_dir = project_root / "src" / "platforms"

    try:
        file.relative_to(platforms_dir)
    except ValueError:
        # Not in platforms/ directory - skip check
        return 0

    # Only require pragma for subdirectory files (implementation details)
    # Root-level files are public by default in IWYU
    if not is_subdirectory_file(file, platforms_dir):
        return 0

    if has_iwyu_pragma(file):
        return 0

    # Missing IWYU pragma on subdirectory file (should be private)
    rel_path = file.relative_to(project_root)
    recommendation = "Should be marked: // IWYU pragma: private"

    print(f"❌ {rel_path}")
    print(f"   → {recommendation}")

    return 1


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check IWYU pragma markings in platform headers"
    )
    parser.add_argument("file", nargs="?", help="Single file to check (optional)")
    args = parser.parse_args()

    # Find platforms directory
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent

    # Single file mode
    if args.file:
        return check_single_file(args.file, project_root)

    # Full directory scan mode
    platforms_dir = project_root / "src" / "platforms"

    if not platforms_dir.exists():
        print(f"❌ Platforms directory not found: {platforms_dir}")
        return 1

    # Find all .h and .hpp files
    headers: list[Path] = []
    for pattern in ["**/*.h", "**/*.hpp"]:
        headers.extend(platforms_dir.glob(pattern))

    if not headers:
        print("✅ No platform headers found")
        return 0

    # Check each file
    errors: list[tuple[Path, str]] = []

    for header in sorted(headers):
        # Only require pragma for subdirectory files (implementation details)
        # Root-level files are public by default in IWYU
        if not is_subdirectory_file(header, platforms_dir):
            continue

        if has_iwyu_pragma(header):
            continue

        # Missing IWYU pragma on subdirectory file (should be private)
        rel_path = header.relative_to(project_root)
        recommendation = "Should be marked: // IWYU pragma: private"

        errors.append((rel_path, recommendation))

    # Report results
    if not errors:
        print("✅ All platform headers have IWYU pragma markings")
        return 0

    print(f"❌ Found {len(errors)} platform header(s) without IWYU pragma:\n")

    for file_path, recommendation in errors:
        print(f"  {file_path}")
        print(f"    → {recommendation}\n")

    return 1


if __name__ == "__main__":
    sys.exit(main())
