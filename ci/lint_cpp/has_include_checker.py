#!/usr/bin/env python3
"""
Linter to ensure all files using FL_HAS_INCLUDE include fl/has_include.h

This prevents compilation errors when FL_HAS_INCLUDE is used without the proper header.
"""

import sys
from pathlib import Path


# Add parent directory to path to import fix_has_include
sys.path.insert(0, str(Path(__file__).parent.parent))  # noqa: SPI001

from fix_has_include import check_has_include, find_files_using_macro  # type: ignore


PROJECT_ROOT = Path(__file__).parent.parent.parent.resolve()


def main() -> int:
    """Check that all files using FL_HAS_INCLUDE include fl/has_include.h."""

    # Find files using the macro
    files_with_macro = find_files_using_macro("FL_HAS_INCLUDE")

    # Check which files have the include
    files_missing_include = {
        f for f in files_with_macro if not check_has_include(f, "fl/has_include.h")
    }

    if not files_missing_include:
        return 0

    # Report errors
    print(
        f"❌ {len(files_missing_include)} files use FL_HAS_INCLUDE without including fl/has_include.h:\n",
        file=sys.stderr,
    )

    for file_path in sorted(files_missing_include):
        relative = file_path.relative_to(PROJECT_ROOT)
        print(f"   • {relative}", file=sys.stderr)

    print(
        f"\n💡 Fix automatically with: uv run python ci/fix_has_include.py --fix\n",
        file=sys.stderr,
    )

    return 1


if __name__ == "__main__":
    sys.exit(main())
