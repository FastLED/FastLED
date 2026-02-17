#!/usr/bin/env python3
"""
Lint check: Validate unity build structure

Validates two aspects of the unity build system:

1. _build.hpp files reference all .cpp.hpp files in their directory
2. _build.hpp files only include immediate children (one level down)

For each directory in src/** with .cpp.hpp files:
    ✅ Must have _build.hpp file
    ✅ Each .cpp.hpp must be referenced in _build.hpp

For each _build.hpp file:
    ✅ Can only include _build.hpp files from immediate subdirectories
    ✅ NOT from grandchildren or deeper (hierarchical structure)

Also validates library.json srcFilter only includes unity build files:
    ✅ Must reference all _build.cpp files
    ✅ Must exclude all other .cpp files

Example .cpp.hpp inclusion:
    src/fl/channels/channel.cpp.hpp
    → Must appear in src/fl/channels/_build.hpp as:
      #include "fl/channels/channel.cpp.hpp"

Example hierarchical _build.hpp inclusion:
    src/platforms/_build.hpp should include:
      ✅ #include "platforms/arm/_build.hpp" (one level down)
      ❌ #include "platforms/arm/d21/_build.hpp" (two levels - should be in platforms/arm/_build.hpp)
"""

import json
import re
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.paths import PROJECT_ROOT


# File and directory name constants
BUILD_HPP = "_build.cpp.hpp"  # Renamed from _build.hpp to _build.cpp.hpp
BUILD_CPP_HPP = "_build.cpp.hpp"
BUILD_CPP = "_build.cpp"
CPP_HPP_PATTERN = "*.cpp.hpp"
SRC_DIR_NAME = "src"

# Regex patterns
BUILD_HPP_INCLUDE_PATTERN = re.compile(
    r'^\s*#include\s+"([^"]+/' + BUILD_HPP + r')"', re.MULTILINE
)
CPP_HPP_INCLUDE_PATTERN = re.compile(r'^\s*#include\s+"[^"]+\.cpp\.hpp"', re.MULTILINE)

# library.json constants
LIBRARY_JSON_FILE = "library.json"
EXPECTED_BUILD_CPP_FILES = {
    "_build.cpp",
    "fl/_build.cpp",
    "platforms/_build.cpp",
    "third_party/_build.cpp",
}
DANGEROUS_WILDCARD_PATTERNS = [
    "+<*.cpp>",
    "+<platforms/**/*.cpp>",
    "+<third_party/**/*.cpp>",
    "+<fl/**/*.cpp>",
]
EXCLUDE_PATTERN = "-<**/*.cpp>"


# Type aliases for complex types
IncludeMap = dict[
    Path, set[Path]
]  # Maps _build.hpp files to their included _build.hpp files
CppHppByDir = dict[Path, list[Path]]  # Maps directories to their .cpp.hpp files


@dataclass
class ScannedData:
    """Data collected during the scan phase."""

    src_dir: Path
    all_build_hpp_files: list[Path]
    all_build_cpp_files: list[Path]
    cpp_hpp_by_dir: CppHppByDir
    build_hpp_includes: IncludeMap


@dataclass
class CheckResult:
    """Result of unity build structure validation."""

    success: bool
    violations: list[str]

    def __bool__(self) -> bool:
        """Allow CheckResult to be used in boolean context."""
        return self.success


def _get_namespace_path(file_path: Path, src_dir: Path) -> str | None:
    """
    Get the namespace path for a file (path after src/).

    Args:
        file_path: Path to the file
        src_dir: Path to the src directory

    Returns:
        Namespace path as string (e.g., "platforms/arm") or None if not in src/
    """
    try:
        src_index = file_path.parts.index(SRC_DIR_NAME)
        namespace_parts = file_path.parts[src_index + 1 : -1]  # Exclude filename
        return "/".join(namespace_parts)
    except (ValueError, IndexError):
        return None


def _calculate_include_depth(included_path: str, expected_prefix: str) -> int:
    """
    Calculate how many levels deep an include is relative to its parent.

    Args:
        included_path: The included file path (e.g., "platforms/arm/d21/_build.hpp")
        expected_prefix: The expected prefix (e.g., "platforms")

    Returns:
        Number of directory levels (0 = wrong prefix, 1 = immediate child, 2+ = grandchild)
    """
    if not included_path.startswith(expected_prefix + "/"):
        return 0  # Doesn't match expected prefix

    # Remove prefix to get relative part
    relative_part = included_path[len(expected_prefix) + 1 :]  # +1 for the "/"

    # Count directory levels (excluding the final _build.hpp)
    relative_parts = Path(relative_part).parts
    return len(relative_parts) - 1  # Subtract 1 for BUILD_HPP itself


def _get_line_number(content: str, match_start: int) -> int:
    """Get the line number for a match position in file content."""
    return content[:match_start].count("\n") + 1


def _check_build_hpp_naming(src_dir: Path) -> list[str]:
    """
    Check for legacy _build.hpp files (all should now be _build.cpp.hpp).

    Returns:
        list of violation strings
    """
    violations: list[str] = []

    # Check for any legacy _build.hpp files
    for build_hpp in src_dir.rglob("_build.hpp"):
        content = build_hpp.read_text(encoding="utf-8")

        # If it includes .cpp.hpp files, it should be renamed
        if CPP_HPP_INCLUDE_PATTERN.search(content):
            rel_file = build_hpp.relative_to(PROJECT_ROOT)
            expected_name = build_hpp.parent / BUILD_CPP_HPP
            rel_expected = expected_name.relative_to(PROJECT_ROOT)
            violations.append(
                f"{rel_file.as_posix()}: Legacy _build.hpp file includes .cpp.hpp files. "
                f"Should be renamed to '{rel_expected.as_posix()}' to follow implementation file convention."
            )

    return violations


def check_hierarchy(src_dir: Path) -> list[str]:
    """
    Check that _build.hpp files only include immediate children (one level down).

    Returns:
        list of violation strings
    """
    violations: list[str] = []

    for build_hpp in src_dir.rglob(BUILD_HPP):
        namespace_path = _get_namespace_path(build_hpp, src_dir)
        if namespace_path is None:
            continue  # Not in src/ directory

        content = build_hpp.read_text(encoding="utf-8")

        for match in BUILD_HPP_INCLUDE_PATTERN.finditer(content):
            included_path = match.group(1)
            depth = _calculate_include_depth(included_path, namespace_path)

            if depth == 0:
                # Include doesn't match expected namespace prefix
                line_num = _get_line_number(content, match.start())
                rel_file = build_hpp.relative_to(PROJECT_ROOT)
                violations.append(
                    f"{rel_file.as_posix()}:{line_num}: "
                    f"Include '{included_path}' doesn't match expected prefix '{namespace_path}/'"
                )
            elif depth != 1:
                # Include is too deep (grandchild or deeper)
                line_num = _get_line_number(content, match.start())
                rel_file = build_hpp.relative_to(PROJECT_ROOT)
                violations.append(
                    f"{rel_file.as_posix()}:{line_num}: "
                    f"Include '{included_path}' is {depth} levels deep, but should be exactly 1 level. "
                    f"Expected pattern: '{namespace_path}/<dir>/_build.hpp'"
                )

    return violations


def _check_cpp_hpp_files(src_dir: Path, cpp_hpp_by_dir: CppHppByDir) -> list[str]:
    """
    Check that all .cpp.hpp files are referenced in their directory's _build.cpp.hpp.

    Returns:
        list of violation strings
    """
    violations: list[str] = []

    for dir_path, cpp_hpp_files in sorted(cpp_hpp_by_dir.items()):
        build_hpp = dir_path / BUILD_HPP

        if not build_hpp.exists():
            rel_dir = dir_path.relative_to(PROJECT_ROOT)
            violations.append(f"Missing {BUILD_HPP} in {rel_dir.as_posix()}/")
            continue

        content = build_hpp.read_text(encoding="utf-8")

        for cpp_hpp in sorted(cpp_hpp_files):
            # Skip the _build.cpp.hpp file itself (shouldn't include itself)
            if cpp_hpp.name == BUILD_HPP:
                continue

            rel_path = cpp_hpp.relative_to(src_dir)
            include_path = rel_path.as_posix()

            if include_path not in content:
                rel_build = build_hpp.relative_to(PROJECT_ROOT)
                violations.append(f"{rel_build.as_posix()}: missing {include_path}")

    return violations


def _build_include_map(src_dir: Path, all_build_hpp_files: list[Path]) -> IncludeMap:
    """
    Build a map of which _build.hpp files are included by other _build.hpp files.

    Returns:
        IncludeMap mapping each _build.hpp to set of _build.hpp files it includes
    """
    build_hpp_includes: IncludeMap = {}

    for build_hpp in all_build_hpp_files:
        content = build_hpp.read_text(encoding="utf-8")
        includes: set[Path] = set()

        for other_build_hpp in all_build_hpp_files:
            if other_build_hpp == build_hpp:
                continue

            # Check both POSIX and Windows-style paths for cross-platform compatibility
            include_path_posix = other_build_hpp.relative_to(src_dir).as_posix()
            include_path_win = str(other_build_hpp.relative_to(src_dir)).replace(
                "/", "\\"
            )

            if include_path_posix in content or include_path_win in content:
                includes.add(other_build_hpp)

        build_hpp_includes[build_hpp] = includes

    return build_hpp_includes


def _should_skip_build_hpp(
    build_hpp: Path,
    build_cpp_dir: Path,
    parent_build_hpp: Path,
    build_hpp_includes: IncludeMap,
    src_dir: Path,
    build_cpp_content: str = "",
) -> bool:
    """
    Determine if a _build.cpp.hpp file should be skipped (already included hierarchically or in a header).

    Returns:
        True if this _build.cpp.hpp should be skipped, False otherwise
    """
    try:
        rel_to_cpp_dir = build_hpp.relative_to(build_cpp_dir)
    except ValueError:
        return True  # Not under this _build.cpp's directory

    # Check if this is the parent _build.cpp.hpp itself
    if rel_to_cpp_dir.parts[0] == BUILD_HPP:
        return False  # Don't skip the parent _build.cpp.hpp

    # This is a subdirectory _build.cpp.hpp
    subdir = build_cpp_dir / rel_to_cpp_dir.parts[0]

    # Skip if subdirectory has its own _build.cpp
    if (subdir / BUILD_CPP).exists():
        return True

    # Check if this _build.cpp.hpp is included in a corresponding .hpp header file
    # (e.g., animartrix2_detail/_build.cpp.hpp might be in animartrix2_detail.hpp)
    build_cpp_hpp_path = build_hpp.relative_to(src_dir).as_posix()
    # Check for a corresponding header file (directory name + .hpp)
    dir_name = build_hpp.parent.name
    potential_header = build_hpp.parent.parent / f"{dir_name}.hpp"
    if potential_header.exists():
        try:
            header_content = potential_header.read_text(encoding="utf-8")
            if build_cpp_hpp_path in header_content:
                return True  # Included in header, skip in _build.cpp
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception:
            pass

    # Check if parent _build.cpp.hpp includes this file (directly or via intermediate)
    if not parent_build_hpp.exists():
        return False

    # Direct inclusion by parent
    if build_hpp in build_hpp_includes.get(parent_build_hpp, set()):
        return True

    # For deeply nested files, find the actual parent _build.cpp.hpp by traversing up
    # Example: fx/2d/animartrix2_detail/_build.cpp.hpp -> look for fx/2d/_build.cpp.hpp
    if len(rel_to_cpp_dir.parts) >= 2:
        # Try each ancestor directory from immediate parent up to top-level
        current_path = build_hpp.parent
        while current_path != build_cpp_dir:
            potential_parent_hpp = current_path / BUILD_HPP
            if potential_parent_hpp.exists() and potential_parent_hpp != build_hpp:
                # Check if this parent is included by the build_cpp's _build.cpp.hpp
                if potential_parent_hpp in build_hpp_includes.get(
                    parent_build_hpp, set()
                ):
                    return True  # Parent includes intermediate, hierarchy ensures chain

                # Also check if the intermediate parent is directly in _build.cpp content
                # This handles cases where _build.cpp includes deep _build.cpp.hpp files directly
                if build_cpp_content:
                    intermediate_path = potential_parent_hpp.relative_to(
                        src_dir
                    ).as_posix()
                    if intermediate_path in build_cpp_content:
                        return True  # Parent is in _build.cpp, hierarchy ensures chain
            current_path = current_path.parent
            if not current_path.is_relative_to(build_cpp_dir):
                break

    return False


def _check_build_cpp_files(
    src_dir: Path,
    all_build_hpp_files: list[Path],
    all_build_cpp_files: list[Path],
    build_hpp_includes: IncludeMap,
) -> list[str]:
    """
    Check that all _build.hpp files are properly included by their _build.cpp files.

    Returns:
        list of violation strings
    """
    violations: list[str] = []

    for build_cpp in all_build_cpp_files:
        build_cpp_dir = build_cpp.parent
        build_content = build_cpp.read_text(encoding="utf-8")
        parent_build_hpp = build_cpp_dir / BUILD_HPP

        for build_hpp in all_build_hpp_files:
            if _should_skip_build_hpp(
                build_hpp,
                build_cpp_dir,
                parent_build_hpp,
                build_hpp_includes,
                src_dir,
                build_content,
            ):
                continue

            # This _build.cpp.hpp should be directly included by _build.cpp
            rel_path = build_hpp.relative_to(src_dir)
            include_path = rel_path.as_posix()

            if include_path not in build_content:
                rel_cpp = build_cpp.relative_to(PROJECT_ROOT)
                violations.append(f"{rel_cpp.as_posix()}: missing {include_path}")

    return violations


def _check_library_json_srcfilter() -> list[str]:
    """
    Check that library.json srcFilter only includes unity build _build.cpp files.

    Returns:
        list of violation strings
    """
    violations: list[str] = []
    library_json = PROJECT_ROOT / LIBRARY_JSON_FILE

    if not library_json.exists():
        violations.append(f"Missing {LIBRARY_JSON_FILE} file")
        return violations

    try:
        with open(library_json, encoding="utf-8") as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        violations.append(f"{LIBRARY_JSON_FILE}: Invalid JSON - {e}")
        return violations

    if "build" not in data or "srcFilter" not in data["build"]:
        violations.append(f"{LIBRARY_JSON_FILE}: Missing build.srcFilter configuration")
        return violations

    src_filter: list[str] = data["build"]["srcFilter"]

    # Find all _build.cpp references in srcFilter
    found_build_files: set[str] = set()
    has_exclude_pattern = False

    for pattern in src_filter:
        if pattern.startswith("+<") and pattern.endswith(">"):
            path = pattern[2:-1]  # Strip "+<" and ">"
            if path.endswith(BUILD_CPP):
                found_build_files.add(path)
        elif pattern == EXCLUDE_PATTERN:
            has_exclude_pattern = True

    # Check all expected _build.cpp files are referenced
    missing = EXPECTED_BUILD_CPP_FILES - found_build_files
    for missing_file in sorted(missing):
        violations.append(
            f"{LIBRARY_JSON_FILE}: Missing +<{missing_file}> in srcFilter"
        )

    # Check for exclude pattern
    if not has_exclude_pattern:
        violations.append(
            f"{LIBRARY_JSON_FILE}: Missing {EXCLUDE_PATTERN} exclude pattern in srcFilter"
        )

    # Check for dangerous wildcard patterns
    for pattern in src_filter:
        if pattern in DANGEROUS_WILDCARD_PATTERNS:
            violations.append(
                f"{LIBRARY_JSON_FILE}: Found wildcard pattern '{pattern}' - "
                "this will compile individual .cpp files instead of unity builds. "
                "Remove this pattern (unity builds are already included)."
            )

    return violations


def scan() -> ScannedData | None:
    """
    Scan phase: Collect all data needed for validation.

    Returns:
        ScannedData if src/ exists, None otherwise
    """
    src_dir = PROJECT_ROOT / SRC_DIR_NAME
    if not src_dir.exists():
        return None

    # 1. Find all _build.hpp files
    all_build_hpp_files = list(src_dir.rglob(BUILD_HPP))

    # 2. Find all _build.cpp files
    all_build_cpp_files = list(src_dir.rglob(BUILD_CPP))

    # 3. Group .cpp.hpp files by directory
    cpp_hpp_by_dir: CppHppByDir = defaultdict(list)
    for cpp_hpp in src_dir.rglob(CPP_HPP_PATTERN):
        cpp_hpp_by_dir[cpp_hpp.parent].append(cpp_hpp)

    # 4. Build include map for _build.hpp files
    build_hpp_includes = _build_include_map(src_dir, all_build_hpp_files)

    return ScannedData(
        src_dir=src_dir,
        all_build_hpp_files=all_build_hpp_files,
        all_build_cpp_files=all_build_cpp_files,
        cpp_hpp_by_dir=cpp_hpp_by_dir,
        build_hpp_includes=build_hpp_includes,
    )


def check_scanned_data(data: ScannedData) -> CheckResult:
    """
    Check phase: Validate the scanned data.

    Args:
        data: ScannedData from the scan phase

    Returns:
        CheckResult with success flag and list of violations
    """
    violations: list[str] = []

    # 1. Check _build.hpp naming (must be _build.cpp.hpp if includes .cpp.hpp)
    violations.extend(_check_build_hpp_naming(data.src_dir))

    # 2. Check hierarchical structure
    violations.extend(check_hierarchy(data.src_dir))

    # 3. Check all .cpp.hpp files are referenced in _build.hpp
    violations.extend(_check_cpp_hpp_files(data.src_dir, data.cpp_hpp_by_dir))

    # 4. Check _build.cpp files include necessary _build.hpp files
    violations.extend(
        _check_build_cpp_files(
            data.src_dir,
            data.all_build_hpp_files,
            data.all_build_cpp_files,
            data.build_hpp_includes,
        )
    )

    # 5. Validate library.json srcFilter
    violations.extend(_check_library_json_srcfilter())

    return CheckResult(success=len(violations) == 0, violations=violations)


def check() -> CheckResult:
    """
    Check unity build structure integrity.

    Convenience function that combines scan and check phases.

    Returns:
        CheckResult with success flag and list of violations
    """
    data = scan()
    if data is None:
        return CheckResult(success=True, violations=[])

    return check_scanned_data(data)


def main() -> int:
    """Run standalone check."""
    result = check()

    if result.success:
        print("✅ Unity build integrity verified:")
        print("  - All .cpp.hpp files referenced in _build.hpp files")
        print("  - _build.hpp hierarchy correctly structured (one level at a time)")
        print("  - All _build.hpp files referenced in master _build.cpp files")
        print("  - library.json srcFilter correctly configured for unity builds")
        return 0
    else:
        print("❌ Unity build violations found:")
        for violation in result.violations:
            print(f"  - {violation}")
        print(f"\nTotal violations: {len(result.violations)}")
        print("\nFixes:")
        print("  - Add missing #include lines to _build.hpp or _build.cpp files")
        print("  - Ensure _build.hpp files only include immediate children")
        print("  - Update library.json srcFilter to only reference _build.cpp files")
        return 1


if __name__ == "__main__":
    import sys

    sys.exit(main())
