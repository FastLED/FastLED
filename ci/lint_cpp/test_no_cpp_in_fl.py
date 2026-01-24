#!/usr/bin/env python3
"""
Lint check: Prevent .cpp files in src/fl/** (unity build enforcement)

After converting src/fl/ to manual unity build pattern, only _build.cpp should
exist as a .cpp file. All implementation files must use .cpp.hpp extension.

Allowed:
    src/fl/_build.cpp             ✅ Master unity build file
    src/fl/**/*.cpp.hpp           ✅ Includable implementation files
    src/fl/**/*.h                 ✅ Header files

Forbidden:
    src/fl/async.cpp              ❌ Should be async.cpp.hpp
    src/fl/channels/channel.cpp   ❌ Should be channel.cpp.hpp
"""

from pathlib import Path

from ci.util.paths import PROJECT_ROOT


def check() -> tuple[bool, list[str]]:
    """
    Check that no .cpp files exist in src/fl/** except _build.cpp

    Returns:
        (success: bool, violations: list[str])
    """
    fl_dir = PROJECT_ROOT / "src" / "fl"
    if not fl_dir.exists():
        return (True, [])

    violations: list[str] = []

    # Find all .cpp files in src/fl/**
    for cpp_file in fl_dir.rglob("*.cpp"):
        # Allow _build.cpp (master unity build file)
        if cpp_file.name == "_build.cpp":
            continue

        # All other .cpp files are violations
        rel_path = cpp_file.relative_to(PROJECT_ROOT)
        violations.append(str(rel_path).replace("\\", "/"))

    return (len(violations) == 0, violations)


def main() -> int:
    """Run standalone check."""
    success, violations = check()

    if success:
        print("✅ Unity build enforced: only _build.cpp found in src/fl/**")
        return 0
    else:
        print("❌ Found .cpp files in src/fl/** that should be .cpp.hpp:")
        for violation in sorted(violations):
            print(f"  - {violation}")
        print(f"\nTotal violations: {len(violations)}")
        print("\nFix: Rename .cpp → .cpp.hpp and add to appropriate _build.hpp")
        return 1


if __name__ == "__main__":
    import sys

    sys.exit(main())
