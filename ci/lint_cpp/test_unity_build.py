#!/usr/bin/env python3
"""
Lint check: Validate unity build _build.hpp files reference all .cpp.hpp files

Ensures all .cpp.hpp implementation files are included in their directory's
_build.hpp file. The compiler will verify they're actually compiled.

For each directory in src/** with .cpp.hpp files:
    ✅ Must have _build.hpp file
    ✅ Each .cpp.hpp must be referenced in _build.hpp

Also validates library.json srcFilter only includes unity build files:
    ✅ Must reference all _build.cpp files
    ✅ Must exclude all other .cpp files

Example:
    src/fl/channels/channel.cpp.hpp
    → Must appear in src/fl/channels/_build.hpp as:
      #include "fl/channels/channel.cpp.hpp"
"""

import json
from collections import defaultdict
from pathlib import Path

from ci.util.paths import PROJECT_ROOT


def check() -> tuple[bool, list[str]]:
    """
    Check that all .cpp.hpp files are referenced in _build.hpp files,
    that all _build.hpp files are referenced in master _build.cpp files,
    and that library.json srcFilter only includes unity build files.

    Returns:
        (success: bool, violations: list[str])
    """
    src_dir = PROJECT_ROOT / "src"
    if not src_dir.exists():
        return (True, [])

    violations: list[str] = []

    # Group .cpp.hpp files by directory across entire src/
    cpp_hpp_by_dir: dict[Path, list[Path]] = defaultdict(list)
    for cpp_hpp in src_dir.rglob("*.cpp.hpp"):
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

    # Build a map of which _build.hpp files are included by other _build.hpp files
    # This handles the cascading pattern: _build.cpp -> subdir/_build.hpp -> subdir/nested/_build.hpp
    build_hpp_includes: dict[Path, set[Path]] = {}
    for build_hpp in all_build_hpp_files:
        build_hpp_content = build_hpp.read_text(encoding="utf-8")
        includes: set[Path] = set()
        for other_build_hpp in all_build_hpp_files:
            if other_build_hpp == build_hpp:
                continue
            include_path = other_build_hpp.relative_to(src_dir).as_posix()
            if include_path in build_hpp_content:
                includes.add(other_build_hpp)
        build_hpp_includes[build_hpp] = includes

    # Check that all _build.hpp files are included in their corresponding _build.cpp
    # Find all _build.cpp files in the source tree
    all_build_cpp_files = list(src_dir.rglob("_build.cpp"))

    for build_cpp in all_build_cpp_files:
        build_cpp_dir = build_cpp.parent
        build_content = build_cpp.read_text(encoding="utf-8")

        # Check all _build.hpp files that should be included by this _build.cpp
        for build_hpp in all_build_hpp_files:
            # Skip if this _build.hpp is not in the same directory or immediate subdirectory
            try:
                rel_to_cpp_dir = build_hpp.relative_to(build_cpp_dir)
            except ValueError:
                continue  # Not under this _build.cpp's directory

            # Only check immediate subdirectories (not nested subdirectories with their own _build.cpp)
            # If the subdirectory has its own _build.cpp, skip it (it will be checked separately)
            # Also skip if the immediate subdirectory has a _build.hpp that acts as an intermediary
            if rel_to_cpp_dir.parts[0] != "_build.hpp":
                # This is a subdirectory _build.hpp
                subdir = build_cpp_dir / rel_to_cpp_dir.parts[0]
                if (subdir / "_build.cpp").exists():
                    # Subdirectory has its own _build.cpp, so parent shouldn't include its _build.hpp
                    continue
                intermediate_build_hpp = subdir / "_build.hpp"
                if intermediate_build_hpp.exists() and len(rel_to_cpp_dir.parts) > 1:
                    # Subdirectory has a _build.hpp that could act as an intermediary
                    # Check if the intermediate _build.hpp actually includes this nested _build.hpp
                    if build_hpp in build_hpp_includes.get(
                        intermediate_build_hpp, set()
                    ):
                        # The intermediate _build.hpp includes the nested one, so skip
                        continue

            # Expected include path: "path/to/_build.hpp" (relative to src/)
            rel_path = build_hpp.relative_to(src_dir)
            include_path = rel_path.as_posix()

            # Check if mentioned in this _build.cpp
            if include_path not in build_content:
                rel_cpp = build_cpp.relative_to(PROJECT_ROOT)
                violations.append(f"{rel_cpp.as_posix()}: missing {include_path}")

    # Validate library.json srcFilter
    violations.extend(_check_library_json_srcfilter())

    return (len(violations) == 0, violations)


def _check_library_json_srcfilter() -> list[str]:
    """
    Check that library.json srcFilter only includes unity build _build.cpp files
    and excludes all other .cpp files.

    Returns:
        list of violation strings
    """
    violations: list[str] = []
    library_json = PROJECT_ROOT / "library.json"

    if not library_json.exists():
        violations.append("Missing library.json file")
        return violations

    try:
        with open(library_json, encoding="utf-8") as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        violations.append(f"library.json: Invalid JSON - {e}")
        return violations

    if "build" not in data or "srcFilter" not in data["build"]:
        violations.append("library.json: Missing build.srcFilter configuration")
        return violations

    src_filter: list[str] = data["build"]["srcFilter"]

    # Expected _build.cpp files
    expected_build_files = {
        "_build.cpp",
        "fl/_build.cpp",
        "platforms/_build.cpp",
        "third_party/_build.cpp",
    }

    # Find all _build.cpp references in srcFilter
    found_build_files: set[str] = set()
    has_exclude_pattern = False

    for pattern in src_filter:
        if pattern.startswith("+<") and pattern.endswith(">"):
            path = pattern[2:-1]  # Strip "+<" and ">"
            if path.endswith("_build.cpp"):
                found_build_files.add(path)
        elif pattern == "-<**/*.cpp>":
            has_exclude_pattern = True

    # Check all expected _build.cpp files are referenced
    missing = expected_build_files - found_build_files
    for missing_file in sorted(missing):
        violations.append(f"library.json: Missing +<{missing_file}> in srcFilter")

    # Check for exclude pattern
    if not has_exclude_pattern:
        violations.append(
            "library.json: Missing -<**/*.cpp> exclude pattern in srcFilter"
        )

    # Check for dangerous wildcard patterns
    dangerous_patterns = [
        "+<*.cpp>",
        "+<platforms/**/*.cpp>",
        "+<third_party/**/*.cpp>",
        "+<fl/**/*.cpp>",
    ]

    for pattern in src_filter:
        if pattern in dangerous_patterns:
            violations.append(
                f"library.json: Found wildcard pattern '{pattern}' - "
                "this will compile individual .cpp files instead of unity builds. "
                "Remove this pattern (unity builds are already included)."
            )

    return violations


def main() -> int:
    """Run standalone check."""
    success, violations = check()

    if success:
        print("✅ Unity build integrity verified:")
        print("  - All .cpp.hpp files referenced in _build.hpp files")
        print("  - All _build.hpp files referenced in master _build.cpp files")
        print("  - library.json srcFilter correctly configured for unity builds")
        return 0
    else:
        print("❌ Unity build violations found:")
        for violation in violations:
            print(f"  - {violation}")
        print(f"\nTotal violations: {len(violations)}")
        print("\nFixes:")
        print("  - Add missing #include lines to _build.hpp or _build.cpp files")
        print("  - Update library.json srcFilter to only reference _build.cpp files")
        return 1


if __name__ == "__main__":
    import sys

    sys.exit(main())
