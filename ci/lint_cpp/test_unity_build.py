#!/usr/bin/env python3
"""
Lint check: Validate unity build structure

Validates two aspects of the unity build system:

1. _build.cpp.hpp files reference all .cpp.hpp files in their directory
2. _build.cpp.hpp files only include immediate children (one level down)

For each directory in src/** with .cpp.hpp files:
    ✅ Must have _build.cpp.hpp file
    ✅ Each .cpp.hpp must be referenced in _build.cpp.hpp

For each _build.cpp.hpp file:
    ✅ Can only include _build.cpp.hpp files from immediate subdirectories
    ✅ NOT from grandchildren or deeper (hierarchical structure)

Build files live in src/fl/build/ using Scheme 5 naming:
    ✅ Dot-delimited path, + suffix = recursive, no + = flat
    ✅ Every build file must include platform pre-headers
    ✅ library.json srcFilter must reference all build files

Example .cpp.hpp inclusion:
    src/fl/channels/channel.cpp.hpp
    → Must appear in src/fl/channels/_build.cpp.hpp as:
      #include "fl/channels/channel.cpp.hpp"

Example hierarchical _build.cpp.hpp inclusion:
    src/platforms/_build.cpp.hpp should include:
      ✅ #include "platforms/arm/_build.cpp.hpp" (one level down)
      ❌ #include "platforms/arm/d21/_build.cpp.hpp" (two levels - should be in platforms/arm/_build.cpp.hpp)
"""

import json
import re
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

from ci.util.paths import PROJECT_ROOT


# File and directory name constants
BUILD_HPP = "_build.cpp.hpp"  # Renamed from _build.hpp to _build.cpp.hpp
BUILD_CPP_HPP = "_build.cpp.hpp"
BUILD_CPP = "_build.cpp"
CPP_HPP_PATTERN = "*.cpp.hpp"
SRC_DIR_NAME = "src"

# Build directory (relative to src/)
BUILD_DIR = "fl/build"

# Regex patterns
BUILD_HPP_INCLUDE_PATTERN = re.compile(
    r'^\s*#include\s+"([^"]+/' + BUILD_HPP + r')"', re.MULTILINE
)
CPP_HPP_INCLUDE_PATTERN = re.compile(r'^\s*#include\s+"[^"]+\.cpp\.hpp"', re.MULTILINE)

# library.json constants
LIBRARY_JSON_FILE = "library.json"
EXPECTED_BUILD_FILES = {
    "fl/build/src.cpp",
    "fl/build/fl.cpp",
    "fl/build/fl.asset+.cpp",
    "fl/build/fl.audio+.cpp",
    "fl/build/fl.channels+.cpp",
    "fl/build/fl.chipsets+.cpp",
    "fl/build/fl.codec+.cpp",
    "fl/build/fl.detail+.cpp",
    "fl/build/fl.details+.cpp",
    "fl/build/fl.font+.cpp",
    "fl/build/fl.fx+.cpp",
    "fl/build/fl.gfx+.cpp",
    "fl/build/fl.math+.cpp",
    "fl/build/fl.net+.cpp",
    "fl/build/fl.control+.cpp",
    "fl/build/fl.remote+.cpp",
    "fl/build/fl.sensors+.cpp",
    "fl/build/fl.stl+.cpp",
    "fl/build/fl.system+.cpp",
    "fl/build/fl.task+.cpp",
    "fl/build/fl.test+.cpp",
    "fl/build/fl.ui+.cpp",
    "fl/build/fl.video+.cpp",
    "fl/build/platforms+.cpp",
    "fl/build/third_party+.cpp",
}
DANGEROUS_WILDCARD_PATTERNS = [
    "+<*.cpp>",
    "+<platforms/**/*.cpp>",
    "+<third_party/**/*.cpp>",
    "+<fl/**/*.cpp>",
]
EXCLUDE_PATTERN = "-<**/*.cpp>"

# Required pre-headers that must appear before any .cpp.hpp includes in build files
REQUIRED_PRE_HEADERS = [
    "platforms/new.h",
    "fl/system/arduino.h",
]


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
    all_build_files: list[Path]  # Build files in src/fl/build/
    cpp_hpp_by_dir: CppHppByDir
    independently_compiled_dirs: set[str] = field(default_factory=lambda: set[str]())


@dataclass
class CheckResult:
    """Result of unity build structure validation."""

    success: bool
    violations: list[str]

    def __bool__(self) -> bool:
        """Allow CheckResult to be used in boolean context."""
        return self.success


def _parse_build_filename(filename: str) -> tuple[str, bool]:
    """
    Parse a build filename into (mapped_directory, is_recursive).

    Args:
        filename: Build filename (e.g., "src.cpp", "fl.audio+.cpp", "platforms+.cpp")

    Returns:
        Tuple of (directory path relative to src/, is_recursive)

    Examples:
        "src.cpp" -> ("", False)           # maps to src/ root, flat
        "fl.cpp" -> ("fl", False)          # maps to src/fl/, flat
        "fl.audio+.cpp" -> ("fl/audio", True)
        "platforms+.cpp" -> ("platforms", True)
        "third_party+.cpp" -> ("third_party", True)
    """
    # Strip .cpp suffix
    stem = filename.removesuffix(".cpp")

    # Check for recursive marker
    is_recursive = stem.endswith("+")
    if is_recursive:
        stem = stem.removesuffix("+")

    # Special case: "src" maps to root src/ directory (empty relative path)
    if stem == "src":
        return ("", is_recursive)

    # Replace dots with slashes to get directory path
    # Handle "third_party" which contains an underscore (not a dot)
    dir_path = stem.replace(".", "/")
    return (dir_path, is_recursive)


def _get_independently_compiled_dirs(src_dir: Path) -> set[str]:
    """
    Determine which directories are independently compiled by build files in src/fl/build/.

    Returns:
        Set of directory paths relative to src/ (e.g., {"", "fl", "fl/audio", "platforms", ...})
    """
    build_dir = src_dir / "fl" / "build"
    if not build_dir.exists():
        return set()

    dirs: set[str] = set()
    for build_file in build_dir.glob("*.cpp"):
        dir_path, _ = _parse_build_filename(build_file.name)
        dirs.add(dir_path)
    return dirs


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
        # Skip the build directory itself — build files aren't tracked by _build.cpp.hpp
        build_dir = src_dir / "fl" / "build"
        if dir_path == build_dir or str(dir_path).startswith(str(build_dir)):
            continue

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


SECTION_COMMENT_CURRENT_DIR = "// begin current directory includes"
SECTION_COMMENT_SUB_DIR = "// begin sub directory includes"


def _check_build_include_order(src_dir: Path) -> list[str]:
    """
    Check that _build.cpp.hpp files have correct include ordering and section comments.

    Required structure (when both same-level AND subdir includes exist):

        // begin current directory includes
        #include "fl/channels/channel.cpp.hpp"
        ...

        // begin sub directory includes
        #include "fl/channels/detail/_build.cpp.hpp"
        ...

    Rules:
    1. Same-level *.cpp.hpp includes BEFORE subdirectory _build.cpp.hpp includes.
    2. When both sections exist, require section comment markers with a blank line before each.
    3. Files with only one kind of include do NOT need section comments.

    Returns:
        list of violation strings
    """
    violations: list[str] = []

    include_pattern = re.compile(r'^\s*#include\s+"([^"]+\.cpp\.hpp)"', re.MULTILINE)

    for build_hpp in src_dir.rglob(BUILD_HPP):
        content = build_hpp.read_text(encoding="utf-8")
        lines = content.splitlines()
        namespace_path = _get_namespace_path(build_hpp, src_dir)
        if namespace_path is None:
            continue

        includes = list(include_pattern.finditer(content))
        if not includes:
            continue

        rel_file = build_hpp.relative_to(PROJECT_ROOT).as_posix()

        # Classify includes
        same_level_lines: list[int] = []
        subdir_lines: list[int] = []

        for match in includes:
            included_path = match.group(1)
            line_num = _get_line_number(content, match.start())

            if included_path.endswith(BUILD_HPP):
                subdir_lines.append(line_num)
            else:
                same_level_lines.append(line_num)

        has_both = bool(same_level_lines) and bool(subdir_lines)

        # Check ordering: all same-level must come before all subdir
        if has_both:
            last_same = max(same_level_lines)
            first_subdir = min(subdir_lines)
            if last_same > first_subdir:
                violations.append(
                    f"{rel_file}: Same-level .cpp.hpp includes (last at line {last_same}) "
                    f"must come BEFORE subdirectory _build.cpp.hpp includes (first at line {first_subdir}). "
                    f"Required order: same-level *.cpp.hpp first, then subdir/_build.cpp.hpp last."
                )

        # Check section comments (only required when both sections exist)
        if not has_both:
            continue

        # Find section comment lines
        current_dir_comment_line: int | None = None
        sub_dir_comment_line: int | None = None
        for i, line in enumerate(lines, 1):
            stripped = line.strip()
            if stripped == SECTION_COMMENT_CURRENT_DIR:
                current_dir_comment_line = i
            elif stripped == SECTION_COMMENT_SUB_DIR:
                sub_dir_comment_line = i

        first_same = min(same_level_lines)
        first_subdir = min(subdir_lines)

        # Check "// begin current directory includes" exists and is placed correctly
        if current_dir_comment_line is None:
            violations.append(
                f'{rel_file}: Missing "{SECTION_COMMENT_CURRENT_DIR}" comment before '
                f"same-level includes (first at line {first_same}). "
                f"Add it on the line immediately before the first same-level #include."
            )
        else:
            # Must be on the line immediately before the first same-level include
            if current_dir_comment_line != first_same - 1:
                violations.append(
                    f'{rel_file}:{current_dir_comment_line}: "{SECTION_COMMENT_CURRENT_DIR}" '
                    f"must be on line {first_same - 1} (immediately before first same-level include "
                    f"at line {first_same}), not line {current_dir_comment_line}."
                )
            # Must have a blank line before the comment (unless it's at the very start of content)
            if current_dir_comment_line >= 2:
                prev_line = lines[
                    current_dir_comment_line - 2
                ]  # -2: 1-indexed + prev line
                if prev_line.strip() != "" and not prev_line.strip().startswith("///"):
                    violations.append(
                        f'{rel_file}:{current_dir_comment_line}: "{SECTION_COMMENT_CURRENT_DIR}" '
                        f"must have a blank line before it (line {current_dir_comment_line - 1} is not blank)."
                    )

        # Check "// begin sub directory includes" exists and is placed correctly
        if sub_dir_comment_line is None:
            violations.append(
                f'{rel_file}: Missing "{SECTION_COMMENT_SUB_DIR}" comment before '
                f"subdirectory includes (first at line {first_subdir}). "
                f"Add it on the line immediately before the first subdirectory #include."
            )
        else:
            # Must be on the line immediately before the first subdir include
            if sub_dir_comment_line != first_subdir - 1:
                violations.append(
                    f'{rel_file}:{sub_dir_comment_line}: "{SECTION_COMMENT_SUB_DIR}" '
                    f"must be on line {first_subdir - 1} (immediately before first subdirectory include "
                    f"at line {first_subdir}), not line {sub_dir_comment_line}."
                )
            # Must have a blank line before the comment
            if sub_dir_comment_line >= 2:
                prev_line = lines[sub_dir_comment_line - 2]
                if prev_line.strip() != "":
                    violations.append(
                        f'{rel_file}:{sub_dir_comment_line}: "{SECTION_COMMENT_SUB_DIR}" '
                        f"must have a blank line before it (line {sub_dir_comment_line - 1} is not blank)."
                    )

    return violations


def _check_subdir_completeness(
    src_dir: Path, independently_compiled_dirs: set[str]
) -> list[str]:
    """
    Check that _build.cpp.hpp files include ALL immediate subdirectory _build.cpp.hpp files.

    For each _build.cpp.hpp, every immediate subdirectory that:
    1. Contains its own _build.cpp.hpp
    2. Is NOT independently compiled by a build file in src/fl/build/

    ...must be included. This catches the case where a new subdirectory is added
    but not included in _build.cpp.hpp, causing linker errors.

    Returns:
        list of violation strings
    """
    violations: list[str] = []

    for build_hpp in src_dir.rglob(BUILD_HPP):
        dir_path = build_hpp.parent
        content = build_hpp.read_text(encoding="utf-8")
        rel_file = build_hpp.relative_to(PROJECT_ROOT).as_posix()

        # Find all immediate subdirs with _build.cpp.hpp
        for subdir in sorted(dir_path.iterdir()):
            if not subdir.is_dir():
                continue
            sub_build = subdir / BUILD_HPP
            if not sub_build.exists():
                continue

            # Skip subdirs that are independently compiled by a build file in src/fl/build/
            try:
                subdir_rel = subdir.relative_to(src_dir).as_posix()
            except ValueError:
                continue
            if subdir_rel in independently_compiled_dirs:
                continue

            # This subdir's _build.cpp.hpp should be included
            include_path = sub_build.relative_to(src_dir).as_posix()
            if include_path not in content:
                violations.append(
                    f"{rel_file}: Missing subdirectory include '{include_path}'. "
                    f"All immediate subdirectories with {BUILD_HPP} must be included."
                )

    return violations


def _check_alphabetical_order(src_dir: Path) -> list[str]:
    """
    Check that includes within each section of _build.cpp.hpp are alphabetically sorted.

    Rules:
    - Same-level *.cpp.hpp includes must be sorted alphabetically.
    - Subdirectory _build.cpp.hpp includes must be sorted alphabetically.
    - .h header includes at the top (before sections) are exempt from sorting.

    Returns:
        list of violation strings
    """
    violations: list[str] = []

    include_pattern = re.compile(r'^\s*#include\s+"([^"]+\.cpp\.hpp)"', re.MULTILINE)

    for build_hpp in src_dir.rglob(BUILD_HPP):
        content = build_hpp.read_text(encoding="utf-8")
        rel_file = build_hpp.relative_to(PROJECT_ROOT).as_posix()

        includes = list(include_pattern.finditer(content))
        if len(includes) < 2:
            continue

        # Split into sections
        same_level: list[tuple[str, int]] = []
        subdir: list[tuple[str, int]] = []
        for match in includes:
            path = match.group(1)
            line_num = _get_line_number(content, match.start())
            if path.endswith(BUILD_HPP):
                subdir.append((path, line_num))
            else:
                same_level.append((path, line_num))

        # Check each section is alphabetically sorted
        for section_name, section in [
            ("current directory", same_level),
            ("sub directory", subdir),
        ]:
            if len(section) < 2:
                continue
            paths = [p for p, _ in section]
            sorted_paths = sorted(paths)
            if paths != sorted_paths:
                # Find first out-of-order pair
                for i in range(1, len(paths)):
                    if paths[i] < paths[i - 1]:
                        line_num = section[i][1]
                        violations.append(
                            f"{rel_file}:{line_num}: {section_name} includes not alphabetically sorted. "
                            f"'{paths[i]}' comes before '{paths[i - 1]}' but should come after it."
                        )
                        break

    return violations


def _check_no_cross_directory_includes(src_dir: Path) -> list[str]:
    """
    Check that _build.cpp.hpp files only include files from their own directory scope.

    Same-level .cpp.hpp includes must come from the same directory as the _build.cpp.hpp.
    Subdirectory _build.cpp.hpp includes must come from immediate child directories.
    No includes from sibling, parent, or unrelated directories.

    Returns:
        list of violation strings
    """
    violations: list[str] = []

    include_pattern = re.compile(r'^\s*#include\s+"([^"]+)"', re.MULTILINE)

    for build_hpp in src_dir.rglob(BUILD_HPP):
        content = build_hpp.read_text(encoding="utf-8")
        rel_file = build_hpp.relative_to(PROJECT_ROOT).as_posix()
        namespace_path = _get_namespace_path(build_hpp, src_dir)
        if namespace_path is None:
            continue

        # Root src/ directory has empty namespace_path — files use bare names
        expected_prefix = (namespace_path + "/") if namespace_path else ""

        for match in include_pattern.finditer(content):
            included_path = match.group(1)
            line_num = _get_line_number(content, match.start())

            # Skip .h header includes (allowed at top for setup)
            if not included_path.endswith(".cpp.hpp"):
                continue

            # Every .cpp.hpp include must start with the directory's namespace prefix
            if expected_prefix and not included_path.startswith(expected_prefix):
                violations.append(
                    f"{rel_file}:{line_num}: Cross-directory include '{included_path}' "
                    f"does not belong to this directory (expected prefix '{expected_prefix}'). "
                    f"_build.cpp.hpp should only include files from its own directory."
                )
                continue

            # For non-_build.cpp.hpp includes, the file must be directly in this directory
            # (no extra path separators after the prefix, except the filename)
            if not included_path.endswith(BUILD_HPP):
                relative_part = included_path[len(expected_prefix) :]
                if "/" in relative_part:
                    violations.append(
                        f"{rel_file}:{line_num}: Include '{included_path}' is from a "
                        f"subdirectory but is not a _build.cpp.hpp. Same-level includes "
                        f"must be directly in '{namespace_path}/', not in a child folder."
                    )

    return violations


def _check_no_invalid_include_types(src_dir: Path) -> list[str]:
    """
    Check that _build.cpp.hpp files only include .cpp.hpp and .h files.

    Disallowed: .cpp, .c, .cc, or any other non-.cpp.hpp/.h file type.
    Angle-bracket includes (e.g., <new>) are ignored (system headers).

    Returns:
        list of violation strings
    """
    violations: list[str] = []

    # Match quoted includes only (not angle-bracket system includes)
    include_pattern = re.compile(r'^\s*#include\s+"([^"]+)"', re.MULTILINE)

    for build_hpp in src_dir.rglob(BUILD_HPP):
        content = build_hpp.read_text(encoding="utf-8")
        rel_file = build_hpp.relative_to(PROJECT_ROOT).as_posix()

        for match in include_pattern.finditer(content):
            included_path = match.group(1)
            line_num = _get_line_number(content, match.start())

            if included_path.endswith(".cpp.hpp"):
                continue  # Valid
            if included_path.endswith(".h") or included_path.endswith(".hpp"):
                continue  # Valid (setup headers)

            violations.append(
                f"{rel_file}:{line_num}: Invalid include type '{included_path}'. "
                f"_build.cpp.hpp should only include *.cpp.hpp and *.h files."
            )

    return violations


# ---------------------------------------------------------------------------
# Build file validation (src/fl/build/)
# ---------------------------------------------------------------------------


def _check_build_file_preheaders(src_dir: Path) -> list[str]:
    """
    Check that every build file in src/fl/build/ includes required platform pre-headers
    before any .cpp.hpp or _build.cpp.hpp includes.

    Required pre-headers (in order):
    1. platforms/new.h — placement new + __cxa_guard_* declarations
    2. fl/system/arduino.h — Arduino.h trampoline + macro cleanup

    Returns:
        list of violation strings
    """
    violations: list[str] = []
    build_dir = src_dir / "fl" / "build"
    if not build_dir.exists():
        return violations

    include_pattern = re.compile(r'^\s*#include\s+"([^"]+)"', re.MULTILINE)

    for build_file in sorted(build_dir.glob("*.cpp")):
        content = build_file.read_text(encoding="utf-8")
        rel_file = build_file.relative_to(PROJECT_ROOT).as_posix()

        includes = list(include_pattern.finditer(content))
        if not includes:
            continue

        # Find the first .cpp.hpp include (the implementation include)
        first_impl_line: int | None = None
        for match in includes:
            if match.group(1).endswith(".cpp.hpp"):
                first_impl_line = _get_line_number(content, match.start())
                break

        if first_impl_line is None:
            violations.append(
                f"{rel_file}: Build file has no .cpp.hpp includes — this file serves no purpose."
            )
            continue

        # Check each required pre-header appears before the first impl include
        for pre_header in REQUIRED_PRE_HEADERS:
            found = False
            for match in includes:
                if match.group(1) == pre_header:
                    pre_line = _get_line_number(content, match.start())
                    if pre_line < first_impl_line:
                        found = True
                    else:
                        violations.append(
                            f"{rel_file}:{pre_line}: Pre-header '{pre_header}' must appear "
                            f"BEFORE first .cpp.hpp include (line {first_impl_line})."
                        )
                        found = True  # Don't also report missing
                    break
            if not found:
                violations.append(
                    f"{rel_file}: Missing required pre-header '#include \"{pre_header}\"'. "
                    f"All build files must include platform pre-headers before implementation includes."
                )

    return violations


def _check_build_file_content(src_dir: Path, cpp_hpp_by_dir: CppHppByDir) -> list[str]:
    """
    Validate the content of each build file in src/fl/build/:
    - Flat files (no +): include ALL .cpp.hpp from mapped dir, no _build.cpp.hpp
    - Recursive files (+): include exactly one _build.cpp.hpp from mapped dir

    Returns:
        list of violation strings
    """
    violations: list[str] = []
    build_dir = src_dir / "fl" / "build"
    if not build_dir.exists():
        return violations

    include_pattern = re.compile(r'^\s*#include\s+"([^"]+\.cpp\.hpp)"', re.MULTILINE)

    for build_file in sorted(build_dir.glob("*.cpp")):
        content = build_file.read_text(encoding="utf-8")
        rel_file = build_file.relative_to(PROJECT_ROOT).as_posix()

        dir_path, is_recursive = _parse_build_filename(build_file.name)

        # Get all .cpp.hpp includes from this build file
        impl_includes: list[str] = []
        for match in include_pattern.finditer(content):
            impl_includes.append(match.group(1))

        if is_recursive:
            # Must include exactly one _build.cpp.hpp from the mapped directory
            expected_build_hpp = (dir_path + "/" + BUILD_HPP) if dir_path else BUILD_HPP
            build_hpp_includes = [i for i in impl_includes if i.endswith(BUILD_HPP)]
            non_build_includes = [i for i in impl_includes if not i.endswith(BUILD_HPP)]

            if len(build_hpp_includes) == 0:
                violations.append(
                    f"{rel_file}: Recursive build file must include '{expected_build_hpp}'."
                )
            elif len(build_hpp_includes) > 1:
                violations.append(
                    f"{rel_file}: Recursive build file must include exactly one _build.cpp.hpp, "
                    f"found {len(build_hpp_includes)}: {build_hpp_includes}"
                )
            elif build_hpp_includes[0] != expected_build_hpp:
                violations.append(
                    f"{rel_file}: Expected include '{expected_build_hpp}', "
                    f"found '{build_hpp_includes[0]}'."
                )

            if non_build_includes:
                violations.append(
                    f"{rel_file}: Recursive build file should only include one _build.cpp.hpp, "
                    f"found extra .cpp.hpp includes: {non_build_includes}"
                )
        else:
            # Flat: must include ALL .cpp.hpp from mapped dir (excluding _build.cpp.hpp)
            # Must NOT include any _build.cpp.hpp
            mapped_dir = src_dir / dir_path if dir_path else src_dir
            if not mapped_dir.exists():
                violations.append(
                    f"{rel_file}: Mapped directory '{dir_path or 'src/'}' does not exist."
                )
                continue

            build_hpp_includes = [i for i in impl_includes if i.endswith(BUILD_HPP)]
            if build_hpp_includes:
                violations.append(
                    f"{rel_file}: Flat build file must NOT include _build.cpp.hpp files "
                    f"(found: {build_hpp_includes}). Use individual .cpp.hpp includes instead."
                )

            # Determine expected .cpp.hpp files from the directory
            expected_files: set[str] = set()
            for cpp_hpp in cpp_hpp_by_dir.get(mapped_dir, []):
                if cpp_hpp.name == BUILD_HPP:
                    continue
                rel_path = cpp_hpp.relative_to(src_dir).as_posix()
                expected_files.add(rel_path)

            actual_files = set(impl_includes) - set(build_hpp_includes)

            # Check completeness
            missing = expected_files - actual_files
            for m in sorted(missing):
                violations.append(f"{rel_file}: Missing include '{m}'.")

            # Check for extras from wrong directories
            extras = actual_files - expected_files
            for e in sorted(extras):
                violations.append(
                    f"{rel_file}: Include '{e}' does not belong in this flat build file "
                    f"(expected only files from '{dir_path or 'src/'}')."
                )

    return violations


def _check_build_file_naming(src_dir: Path) -> list[str]:
    """
    Validate that every .cpp file in src/fl/build/ follows the naming convention
    and maps to a valid directory.

    Returns:
        list of violation strings
    """
    violations: list[str] = []
    build_dir = src_dir / "fl" / "build"
    if not build_dir.exists():
        violations.append("Missing build directory: src/fl/build/")
        return violations

    actual_files = {f.name for f in build_dir.glob("*.cpp")}
    expected_files = {f.split("/")[-1] for f in EXPECTED_BUILD_FILES}

    # Check for unexpected files
    unexpected = actual_files - expected_files
    for f in sorted(unexpected):
        violations.append(
            f"src/fl/build/{f}: Unexpected build file. "
            f"Not in EXPECTED_BUILD_FILES list."
        )

    # Check for missing files
    missing = expected_files - actual_files
    for f in sorted(missing):
        violations.append(f"src/fl/build/{f}: Expected build file is missing.")

    # Validate each file maps to a valid directory
    for build_file in sorted(build_dir.glob("*.cpp")):
        dir_path, _ = _parse_build_filename(build_file.name)
        mapped_dir = src_dir / dir_path if dir_path else src_dir
        if not mapped_dir.exists():
            rel_file = build_file.relative_to(PROJECT_ROOT).as_posix()
            violations.append(
                f"{rel_file}: Maps to non-existent directory '{dir_path or 'src/'}'."
            )

    return violations


def _check_no_orphan_cpp_files(src_dir: Path) -> list[str]:
    """
    Check that no .cpp files exist in src/ except in src/fl/build/.

    All implementation code must use .cpp.hpp extension. Only the build files
    in src/fl/build/ are allowed to be .cpp files.

    Returns:
        list of violation strings
    """
    violations: list[str] = []
    build_dir = src_dir / "fl" / "build"

    for cpp_file in src_dir.rglob("*.cpp"):
        # Skip .cpp.hpp files
        if cpp_file.name.endswith(".cpp.hpp"):
            continue

        # Allow files in the build directory
        try:
            cpp_file.relative_to(build_dir)
            continue  # It's in build dir, OK
        except ValueError:
            pass

        rel_file = cpp_file.relative_to(PROJECT_ROOT).as_posix()
        violations.append(
            f"{rel_file}: .cpp file found outside src/fl/build/. "
            f"All .cpp files must be in src/fl/build/ (unity build entry points). "
            f"Implementation files should use .cpp.hpp extension."
        )

    return violations


EXPECTED_SRC_FILTER_PATTERN = "+<fl/build/*.cpp>"


def _check_library_json_srcfilter() -> list[str]:
    """
    Check that library.json srcFilter uses the build glob pattern.

    Required patterns:
    - "-<**/*.cpp>" to exclude all .cpp files
    - "+<fl/build/*.cpp>" to include all build files

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

    has_exclude_pattern = False
    has_build_glob = False

    for pattern in src_filter:
        if pattern == EXCLUDE_PATTERN:
            has_exclude_pattern = True
        elif pattern == EXPECTED_SRC_FILTER_PATTERN:
            has_build_glob = True

    # Check for exclude pattern
    if not has_exclude_pattern:
        violations.append(
            f"{LIBRARY_JSON_FILE}: Missing {EXCLUDE_PATTERN} exclude pattern in srcFilter"
        )

    # Check for build glob pattern
    if not has_build_glob:
        violations.append(
            f"{LIBRARY_JSON_FILE}: Missing {EXPECTED_SRC_FILTER_PATTERN} in srcFilter. "
            f"This pattern auto-discovers all build files in src/fl/build/."
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

    # 1. Find all _build.cpp.hpp files
    all_build_hpp_files = list(src_dir.rglob(BUILD_HPP))

    # 2. Find all build files in src/fl/build/
    build_dir = src_dir / "fl" / "build"
    if build_dir.exists():
        all_build_files = list(build_dir.glob("*.cpp"))
    else:
        all_build_files = []

    # 3. Group .cpp.hpp files by directory
    cpp_hpp_by_dir: CppHppByDir = defaultdict(list)
    for cpp_hpp in src_dir.rglob(CPP_HPP_PATTERN):
        cpp_hpp_by_dir[cpp_hpp.parent].append(cpp_hpp)

    # 4. Compute independently compiled directories
    independently_compiled_dirs = _get_independently_compiled_dirs(src_dir)

    return ScannedData(
        src_dir=src_dir,
        all_build_hpp_files=all_build_hpp_files,
        all_build_files=all_build_files,
        cpp_hpp_by_dir=cpp_hpp_by_dir,
        independently_compiled_dirs=independently_compiled_dirs,
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

    # 3. Check all .cpp.hpp files are referenced in _build.cpp.hpp
    violations.extend(_check_cpp_hpp_files(data.src_dir, data.cpp_hpp_by_dir))

    # 3b. Check _build.cpp.hpp include ordering (same-level first, subdir last)
    violations.extend(_check_build_include_order(data.src_dir))

    # 3c. Check _build.cpp.hpp includes ALL immediate subdirectory _build.cpp.hpp files
    violations.extend(
        _check_subdir_completeness(data.src_dir, data.independently_compiled_dirs)
    )

    # 3d. Check alphabetical ordering within sections
    violations.extend(_check_alphabetical_order(data.src_dir))

    # 4. Check no cross-directory includes (files from wrong directory)
    violations.extend(_check_no_cross_directory_includes(data.src_dir))

    # 5. Check no invalid include types (.cpp, .c, etc.)
    violations.extend(_check_no_invalid_include_types(data.src_dir))

    # 6. Validate build files in src/fl/build/
    violations.extend(_check_build_file_naming(data.src_dir))
    violations.extend(_check_build_file_preheaders(data.src_dir))
    violations.extend(_check_build_file_content(data.src_dir, data.cpp_hpp_by_dir))
    violations.extend(_check_no_orphan_cpp_files(data.src_dir))

    # 7. Validate library.json srcFilter
    violations.extend(_check_library_json_srcfilter())

    return CheckResult(success=len(violations) == 0, violations=violations)


def check_single_file(file_path: Path) -> CheckResult:
    """
    Targeted unity build check for a single file.

    Used in single-file linting mode (agent hook on save). Only checks the
    immediate directory — does not walk the full project tree.

    Rules:
    - If file is _build.cpp.hpp: all *.cpp.hpp files in the same directory
      must be listed in it.
    - If file is any other *.cpp.hpp: the _build.cpp.hpp in the same directory
      must include it.
    - If file is in src/fl/build/: validate preheaders and content.

    Returns:
        CheckResult with success flag and list of violations
    """
    if not file_path.name.endswith((".cpp.hpp", ".cpp")):
        return CheckResult(success=True, violations=[])

    src_dir = PROJECT_ROOT / SRC_DIR_NAME
    build_dir = src_dir / "fl" / "build"

    # Handle build files in src/fl/build/
    try:
        file_path.relative_to(build_dir)
        # This is a build file — validate it
        cpp_hpp_by_dir: CppHppByDir = defaultdict(list)
        for cpp_hpp in src_dir.rglob(CPP_HPP_PATTERN):
            cpp_hpp_by_dir[cpp_hpp.parent].append(cpp_hpp)
        violations: list[str] = []
        violations.extend(_check_build_file_preheaders(src_dir))
        violations.extend(_check_build_file_content(src_dir, cpp_hpp_by_dir))
        return CheckResult(success=len(violations) == 0, violations=violations)
    except ValueError:
        pass  # Not in build dir

    if not file_path.name.endswith(".cpp.hpp"):
        return CheckResult(success=True, violations=[])

    dir_path = file_path.parent

    if file_path.name == BUILD_CPP_HPP:
        # Editing a _build.cpp.hpp: check all sibling .cpp.hpp files are listed.
        cpp_hpp_by_dir = {dir_path: list(dir_path.glob(CPP_HPP_PATTERN))}
        violations = _check_cpp_hpp_files(src_dir, cpp_hpp_by_dir)
    else:
        # Editing a regular .cpp.hpp: check its parent _build.cpp.hpp includes it.
        build_hpp = dir_path / BUILD_CPP_HPP

        if not build_hpp.exists():
            try:
                rel_dir = dir_path.relative_to(PROJECT_ROOT)
            except ValueError:
                return CheckResult(success=True, violations=[])
            violations = [f"Missing {BUILD_CPP_HPP} in {rel_dir.as_posix()}/"]
        else:
            try:
                rel_path = file_path.relative_to(src_dir)
            except ValueError:
                return CheckResult(success=True, violations=[])
            include_path = rel_path.as_posix()
            content = build_hpp.read_text(encoding="utf-8")
            if include_path not in content:
                rel_build = build_hpp.relative_to(PROJECT_ROOT)
                violations = [f"{rel_build.as_posix()}: missing {include_path}"]
            else:
                violations = []

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
        print("  - All .cpp.hpp files referenced in _build.cpp.hpp files")
        print("  - _build.cpp.hpp hierarchy correctly structured (one level at a time)")
        print("  - Build files in src/fl/build/ correctly configured")
        print("  - Platform pre-headers present in all build files")
        print("  - library.json srcFilter correctly configured")
        return 0
    else:
        print("❌ Unity build violations found:")
        for violation in result.violations:
            print(f"  - {violation}")
        print(f"\nTotal violations: {len(result.violations)}")
        print("\nFixes:")
        print("  - Add missing #include lines to _build.cpp.hpp files")
        print("  - Ensure _build.cpp.hpp files only include immediate children")
        print("  - Ensure build files in src/fl/build/ have platform pre-headers")
        print("  - Update library.json srcFilter to reference all build files")
        return 1


if __name__ == "__main__":
    import sys

    sys.exit(main())
