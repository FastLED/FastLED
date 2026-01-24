#!/usr/bin/env python3
"""
Lint check: Validate unity build _build.hpp files reference all .cpp.hpp files

Ensures all .cpp.hpp implementation files are included in their directory's
_build.hpp file. The compiler will verify they're actually compiled.

For each directory in src/fl/** with .cpp.hpp files:
    ✅ Must have _build.hpp file
    ✅ Each .cpp.hpp must be referenced in _build.hpp

Example:
    src/fl/channels/channel.cpp.hpp
    → Must appear in src/fl/channels/_build.hpp as:
      #include "fl/channels/channel.cpp.hpp"
"""

from collections import defaultdict
from pathlib import Path

from ci.util.paths import PROJECT_ROOT


def check() -> tuple[bool, list[str]]:
    """
    Check that all .cpp.hpp files are referenced in _build.hpp files
    and that all _build.hpp files are referenced in master _build.cpp

    Returns:
        (success: bool, violations: list[str])
    """
    fl_dir = PROJECT_ROOT / "src" / "fl"
    if not fl_dir.exists():
        return (True, [])

    violations: list[str] = []

    # Group .cpp.hpp files by directory
    cpp_hpp_by_dir: dict[Path, list[Path]] = defaultdict(list)
    for cpp_hpp in fl_dir.rglob("*.cpp.hpp"):
        cpp_hpp_by_dir[cpp_hpp.parent].append(cpp_hpp)

    # Track all _build.hpp files
    all_build_hpp_files: list[Path] = []

    # Check each directory
    for dir_path, cpp_hpp_files in sorted(cpp_hpp_by_dir.items()):
        build_hpp = dir_path / "_build.hpp"

        # Check _build.hpp exists
        if not build_hpp.exists():
            rel_dir = dir_path.relative_to(PROJECT_ROOT)
            violations.append(f"Missing _build.hpp in {rel_dir.as_posix()}/")
            continue

        all_build_hpp_files.append(build_hpp)

        # Read _build.hpp content
        content = build_hpp.read_text(encoding="utf-8")

        # Check each .cpp.hpp file is mentioned
        for cpp_hpp in sorted(cpp_hpp_files):
            # Expected include path: "fl/path/to/file.cpp.hpp"
            rel_path = cpp_hpp.relative_to(PROJECT_ROOT / "src")
            include_path = rel_path.as_posix()

            # Check if mentioned in _build.hpp (as #include "fl/...")
            if include_path not in content:
                rel_build = build_hpp.relative_to(PROJECT_ROOT)
                violations.append(f"{rel_build.as_posix()}: missing {include_path}")

    # Check that all _build.hpp files are included in master _build.cpp
    master_build_cpp = fl_dir / "_build.cpp"
    if master_build_cpp.exists():
        master_content = master_build_cpp.read_text(encoding="utf-8")

        for build_hpp in all_build_hpp_files:
            # Expected include path: "fl/path/to/_build.hpp"
            rel_path = build_hpp.relative_to(PROJECT_ROOT / "src")
            include_path = rel_path.as_posix()

            # Check if mentioned in master _build.cpp
            if include_path not in master_content:
                violations.append(f"src/fl/_build.cpp: missing {include_path}")
    else:
        violations.append("Missing master src/fl/_build.cpp file")

    return (len(violations) == 0, violations)


def main() -> int:
    """Run standalone check."""
    success, violations = check()

    if success:
        print("✅ Unity build integrity verified:")
        print("  - All .cpp.hpp files referenced in _build.hpp files")
        print("  - All _build.hpp files referenced in master _build.cpp")
        return 0
    else:
        print("❌ Unity build violations found:")
        for violation in violations:
            print(f"  - {violation}")
        print(f"\nTotal violations: {len(violations)}")
        print("\nFix: Add missing #include lines to _build.hpp or _build.cpp files")
        return 1


if __name__ == "__main__":
    import sys

    sys.exit(main())
