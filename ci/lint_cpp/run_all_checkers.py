#!/usr/bin/env python3
"""
Unified C++ linter runner.

Loads all checker modules and runs them in a single file-loading pass.
This dramatically improves performance by loading each file only once
instead of K times (where K is the number of checkers).
"""

import os
import sys
from pathlib import Path
from typing import Any

from ci.lint_cpp.attribute_checker import AttributeChecker
from ci.lint_cpp.banned_headers_checker import (
    BANNED_HEADERS_COMMON,
    BANNED_HEADERS_CORE,
    BANNED_HEADERS_PLATFORMS,
    BannedHeadersChecker,
)
from ci.lint_cpp.banned_macros_checker import BannedMacrosChecker
from ci.lint_cpp.banned_namespace_checker import BannedNamespaceChecker

# Import all checker classes
from ci.lint_cpp.check_namespace_includes import NamespaceIncludesChecker
from ci.lint_cpp.check_platform_includes import PlatformTrampolineChecker
from ci.lint_cpp.check_platforms_fl_namespace import PlatformsFlNamespaceChecker
from ci.lint_cpp.check_using_namespace import UsingNamespaceChecker
from ci.lint_cpp.cpp_hpp_includes_checker import CppHppIncludesChecker
from ci.lint_cpp.cpp_include_checker import CppIncludeChecker
from ci.lint_cpp.fastled_header_usage_checker import FastLEDHeaderUsageChecker
from ci.lint_cpp.fl_is_defined_checker import FlIsDefinedChecker
from ci.lint_cpp.google_member_style_checker import GoogleMemberStyleChecker
from ci.lint_cpp.headers_exist_checker import HeadersExistChecker
from ci.lint_cpp.include_after_namespace_checker import IncludeAfterNamespaceChecker
from ci.lint_cpp.include_paths_checker import IncludePathsChecker
from ci.lint_cpp.is_header_include_checker import IsHeaderIncludeChecker
from ci.lint_cpp.iwyu_pragma_block_checker import IwyuPragmaBlockChecker
from ci.lint_cpp.logging_in_iram_checker import LoggingInIramChecker
from ci.lint_cpp.namespace_platforms_checker import NamespacePlatformsChecker
from ci.lint_cpp.native_platform_defines_checker import NativePlatformDefinesChecker
from ci.lint_cpp.no_cpp_in_fl_checker import NoCppInFlChecker
from ci.lint_cpp.no_namespace_fl_declaration import NamespaceFlDeclarationChecker
from ci.lint_cpp.no_using_namespace_fl_in_headers import UsingNamespaceFlChecker
from ci.lint_cpp.numeric_limit_macros_checker import NumericLimitMacroChecker
from ci.lint_cpp.platform_includes_checker import PlatformIncludesChecker
from ci.lint_cpp.reinterpret_cast_checker import ReinterpretCastChecker
from ci.lint_cpp.relative_include_checker import RelativeIncludeChecker
from ci.lint_cpp.serial_printf_checker import SerialPrintfChecker
from ci.lint_cpp.static_in_headers_checker import StaticInHeaderChecker
from ci.lint_cpp.std_namespace_checker import StdNamespaceChecker
from ci.lint_cpp.stdint_type_checker import (
    StdintTypeChecker,
)
from ci.lint_cpp.test_path_structure_checker import TestPathStructureChecker
from ci.lint_cpp.test_unity_build import check as check_unity_build
from ci.lint_cpp.test_unity_build import (
    check_single_file as check_unity_build_single_file,
)
from ci.lint_cpp.unit_test_checker import UnitTestChecker
from ci.lint_cpp.using_namespace_fl_in_examples_checker import (
    UsingNamespaceFlInExamplesChecker,
)
from ci.lint_cpp.weak_attribute_checker import WeakAttributeChecker
from ci.util.check_files import (
    CheckerResults,
    FileContentChecker,
    MultiCheckerFileProcessor,
    collect_files_to_check,
)
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"
EXAMPLES_ROOT = PROJECT_ROOT / "examples"
TESTS_ROOT = PROJECT_ROOT / "tests"


def collect_all_files_by_directory() -> dict[str, list[str]]:
    """Collect files organized by directory for targeted checker application."""
    files_by_dir: dict[str, list[str]] = {}

    # Collect src/ files (all subdirectories)
    files_by_dir["src"] = collect_files_to_check([str(SRC_ROOT)])

    # Collect examples/ files (including .ino files)
    files_by_dir["examples"] = collect_files_to_check(
        [str(EXAMPLES_ROOT)], extensions=[".cpp", ".h", ".hpp", ".ino"]
    )

    # Collect tests/ files
    files_by_dir["tests"] = collect_files_to_check([str(TESTS_ROOT)])

    # Collect specific subdirectories for targeted checking
    fl_dir = SRC_ROOT / "fl"
    if fl_dir.exists():
        files_by_dir["fl"] = collect_files_to_check([str(fl_dir)])

    lib8tion_dir = SRC_ROOT / "lib8tion"
    if lib8tion_dir.exists():
        files_by_dir["lib8tion"] = collect_files_to_check([str(lib8tion_dir)])

    fx_dir = SRC_ROOT / "fx"
    if fx_dir.exists():
        files_by_dir["fx"] = collect_files_to_check([str(fx_dir)])

    sensors_dir = SRC_ROOT / "sensors"
    if sensors_dir.exists():
        files_by_dir["sensors"] = collect_files_to_check([str(sensors_dir)])

    platforms_shared_dir = SRC_ROOT / "platforms" / "shared"
    if platforms_shared_dir.exists():
        files_by_dir["platforms_shared"] = collect_files_to_check(
            [str(platforms_shared_dir)]
        )

    platforms_dir = SRC_ROOT / "platforms"
    if platforms_dir.exists():
        files_by_dir["platforms"] = collect_files_to_check([str(platforms_dir)])

    third_party_dir = SRC_ROOT / "third_party"
    if third_party_dir.exists():
        files_by_dir["third_party"] = collect_files_to_check([str(third_party_dir)])

    return files_by_dir


def create_checkers(
    all_headers: frozenset[str] | None = None,
) -> dict[str, list[FileContentChecker]]:
    """Create all checker instances organized by which files they should check.

    Args:
        all_headers: Optional frozenset of all header file paths in the project
                    (used for cross-file validation in some checkers)
    """
    checkers_by_scope: dict[str, list[FileContentChecker]] = {}

    # Global checkers (run on all src/, examples/, tests/ files)
    checkers_by_scope["global"] = [
        CppIncludeChecker(),
        CppHppIncludesChecker(),
        IncludeAfterNamespaceChecker(),
        GoogleMemberStyleChecker(),
        NumericLimitMacroChecker(),
        StaticInHeaderChecker(),
        LoggingInIramChecker(),
        PlatformIncludesChecker(),
        PlatformTrampolineChecker(),  # Enforce trampoline architecture in src/fl/** and root src/
        WeakAttributeChecker(),
        AttributeChecker(),  # Checks all C++ standard attributes (replaces MaybeUnusedChecker)
        BannedMacrosChecker(),  # Checks for banned preprocessor macros like __has_include
        FlIsDefinedChecker(),
        BannedNamespaceChecker(),  # Checks for banned namespace patterns like fl::fl
        # Note: Private libc++ headers checking is now integrated into BannedHeadersChecker
        # Note: _build.hpp hierarchy checking is now integrated into test_unity_build.py
    ]

    # Src-only checkers
    checkers_by_scope["src"] = [
        UsingNamespaceFlChecker(),
        StdNamespaceChecker(),
        NamespaceIncludesChecker(),
        ReinterpretCastChecker(),
        RelativeIncludeChecker(),
        FastLEDHeaderUsageChecker(),
        StdintTypeChecker(),  # Covers all src/ (excludes third_party/ internally)
    ]

    # Platforms-specific checkers
    checkers_by_scope["platforms"] = [
        PlatformsFlNamespaceChecker(),
        NamespacePlatformsChecker(),
        IsHeaderIncludeChecker(),
        IwyuPragmaBlockChecker(all_headers=all_headers),
    ]

    # Native platform defines checker — runs on ALL src/ files (including third_party/)
    # (the checker internally filters to src/ and excludes dispatch headers,
    # is_*.h detection headers, etc.)
    checkers_by_scope["native_platform_defines"] = [
        NativePlatformDefinesChecker(),
    ]

    # Include paths checker (for fl/ and platforms/ directories)
    # Ensures include paths use "fl/", "platforms/", etc. prefixes
    checkers_by_scope["fl_platforms_include_paths"] = [
        IncludePathsChecker(),
    ]

    # Src root-only checkers
    checkers_by_scope["src_root"] = [
        NamespaceFlDeclarationChecker(),
    ]

    # Examples-only checkers
    checkers_by_scope["examples"] = [
        SerialPrintfChecker(),
        UsingNamespaceFlInExamplesChecker(),
    ]

    # fl/ directory checkers with STRICT enforcement
    checkers_by_scope["fl"] = [
        BannedHeadersChecker(banned_headers_list=BANNED_HEADERS_CORE, strict_mode=True),
        NoCppInFlChecker(),
        IwyuPragmaBlockChecker(all_headers=all_headers),
    ]

    # lib8tion/ directory checkers with STRICT enforcement
    checkers_by_scope["lib8tion"] = [
        BannedHeadersChecker(banned_headers_list=BANNED_HEADERS_CORE, strict_mode=True),
    ]

    # fx/, sensors/, platforms/shared/ checkers
    checkers_by_scope["fx_sensors_platforms_shared"] = [
        BannedHeadersChecker(
            banned_headers_list=BANNED_HEADERS_CORE, strict_mode=False
        ),
    ]

    # platforms/ directory checkers
    checkers_by_scope["platforms_banned"] = [
        BannedHeadersChecker(
            banned_headers_list=BANNED_HEADERS_PLATFORMS, strict_mode=False
        ),
    ]

    # examples/ directory checkers
    checkers_by_scope["examples_banned"] = [
        BannedHeadersChecker(
            banned_headers_list=BANNED_HEADERS_COMMON, strict_mode=False
        ),
    ]

    # third_party/ directory checkers
    checkers_by_scope["third_party"] = [
        BannedHeadersChecker(
            banned_headers_list=BANNED_HEADERS_COMMON, strict_mode=True
        ),
    ]

    # Specialized checker for src/fl/ header files only
    checkers_by_scope["fl_headers"] = [
        UsingNamespaceChecker(),
    ]

    # Tests-only checkers - FL compatibility now enforced
    checkers_by_scope["tests"] = [
        HeadersExistChecker(),
        BannedHeadersChecker(
            banned_headers_list=BANNED_HEADERS_CORE, strict_mode=False
        ),
        StdNamespaceChecker(),
        TestPathStructureChecker(),
        UnitTestChecker(),
    ]

    return checkers_by_scope


def run_checkers(
    files_by_dir: dict[str, list[str]],
    checkers_by_scope: dict[str, list[FileContentChecker]],
) -> dict[str, CheckerResults]:
    """Run all checkers - MULTI-PASS approach for performance (minimizes should_process_file() calls)."""
    processor = MultiCheckerFileProcessor()
    all_results: dict[str, CheckerResults] = {}

    # MULTI-PASS: Process each scope separately with pre-filtered files
    # This minimizes the number of should_process_file() calls compared to single-pass

    # Run global checkers on src/, examples/, tests/
    global_files = (
        files_by_dir.get("src", [])
        + files_by_dir.get("examples", [])
        + files_by_dir.get("tests", [])
    )
    global_checkers = checkers_by_scope.get("global", [])
    if global_files and global_checkers:
        processor.process_files_with_checkers(global_files, global_checkers)

    # Run src-only checkers
    src_files = files_by_dir.get("src", [])
    src_checkers = checkers_by_scope.get("src", [])
    if src_files and src_checkers:
        processor.process_files_with_checkers(src_files, src_checkers)

    # Run platforms checkers
    platforms_files = files_by_dir.get("platforms", [])
    platforms_checkers = checkers_by_scope.get("platforms", [])
    if platforms_files and platforms_checkers:
        processor.process_files_with_checkers(platforms_files, platforms_checkers)

    # Run native platform defines checker on ALL src/ files
    # (checker internally filters to relevant files and excludes dispatch headers)
    native_defines_checkers = checkers_by_scope.get("native_platform_defines", [])
    if src_files and native_defines_checkers:
        processor.process_files_with_checkers(src_files, native_defines_checkers)

    # Run src root checkers
    src_root_checkers = checkers_by_scope.get("src_root", [])
    if src_root_checkers:
        # Collect only files directly in src/ (not subdirectories)
        src_root_files = [f for f in src_files if Path(f).parent.name == "src"]
        if src_root_files:
            processor.process_files_with_checkers(src_root_files, src_root_checkers)

    # Run examples-only checkers
    examples_files = files_by_dir.get("examples", [])
    examples_checkers = checkers_by_scope.get("examples", [])
    if examples_files and examples_checkers:
        processor.process_files_with_checkers(examples_files, examples_checkers)

    # Run fl/ directory checkers
    fl_files = files_by_dir.get("fl", [])
    fl_checkers = checkers_by_scope.get("fl", [])
    if fl_files and fl_checkers:
        processor.process_files_with_checkers(fl_files, fl_checkers)

    # Run fl/ header-only checker
    fl_headers_checkers = checkers_by_scope.get("fl_headers", [])
    if fl_files and fl_headers_checkers:
        fl_header_files = [
            f for f in fl_files if f.endswith((".h", ".hpp", ".hxx", ".hh"))
        ]
        if fl_header_files:
            processor.process_files_with_checkers(fl_header_files, fl_headers_checkers)

    # Run include paths checker on fl/ and platforms/
    fl_platforms_files = fl_files + platforms_files
    fl_platforms_include_paths_checkers = checkers_by_scope.get(
        "fl_platforms_include_paths", []
    )
    if fl_platforms_files and fl_platforms_include_paths_checkers:
        processor.process_files_with_checkers(
            fl_platforms_files, fl_platforms_include_paths_checkers
        )

    # Run lib8tion/ directory checkers
    lib8tion_files = files_by_dir.get("lib8tion", [])
    lib8tion_checkers = checkers_by_scope.get("lib8tion", [])
    if lib8tion_files and lib8tion_checkers:
        processor.process_files_with_checkers(lib8tion_files, lib8tion_checkers)

    # Run fx/, sensors/, platforms/shared/ checkers
    fx_sensors_platforms_files = (
        files_by_dir.get("fx", [])
        + files_by_dir.get("sensors", [])
        + files_by_dir.get("platforms_shared", [])
    )
    fx_sensors_platforms_checkers = checkers_by_scope.get(
        "fx_sensors_platforms_shared", []
    )
    if fx_sensors_platforms_files and fx_sensors_platforms_checkers:
        processor.process_files_with_checkers(
            fx_sensors_platforms_files, fx_sensors_platforms_checkers
        )

    # Run platforms/ banned headers checker
    platforms_banned_checkers = checkers_by_scope.get("platforms_banned", [])
    if platforms_files and platforms_banned_checkers:
        processor.process_files_with_checkers(
            platforms_files, platforms_banned_checkers
        )

    # Run examples banned headers checker
    examples_banned_checkers = checkers_by_scope.get("examples_banned", [])
    if examples_files and examples_banned_checkers:
        processor.process_files_with_checkers(examples_files, examples_banned_checkers)

    # Run third_party checkers
    third_party_files = files_by_dir.get("third_party", [])
    third_party_checkers = checkers_by_scope.get("third_party", [])
    if third_party_files and third_party_checkers:
        processor.process_files_with_checkers(third_party_files, third_party_checkers)

    # Run tests-only checkers
    tests_files = files_by_dir.get("tests", [])
    tests_checkers = checkers_by_scope.get("tests", [])
    if tests_files and tests_checkers:
        processor.process_files_with_checkers(tests_files, tests_checkers)

    # Collect all violations from all checkers
    all_checkers: list[FileContentChecker] = []
    for scope_checkers in checkers_by_scope.values():
        all_checkers.extend(scope_checkers)

    return _collect_violations_from_checkers(all_checkers)


def format_and_print_results(results: dict[str, CheckerResults]) -> int:
    """Format and print all violations. Returns exit code (0 = success, 1 = failures)."""
    total_violations = 0

    for checker_name, checker_results in sorted(results.items()):
        if not checker_results.has_violations():
            continue

        file_count = checker_results.file_count()
        violation_count = checker_results.total_violations()
        total_violations += violation_count

        print(f"\n{'=' * 80}")
        print(
            f"[{checker_name}] Found {violation_count} violation(s) in {file_count} file(s):"
        )
        print("=" * 80)

        for file_path in sorted(checker_results.violations.keys()):
            rel_path = os.path.relpath(file_path, PROJECT_ROOT)
            file_violations = checker_results.violations[file_path]

            print(f"\n{rel_path}:")
            for violation in file_violations.violations:
                print(f"  Line {violation.line_number}: {violation.content}")

    if total_violations > 0:
        print(f"\n{'=' * 80}")
        print(f"❌ Total violations: {total_violations}")
        print("=" * 80)
        return 1
    else:
        print("\n" + "=" * 80)
        print("✅ All C++ linting checks passed!")
        print("=" * 80)
        return 0


def run_unity_build_check() -> tuple[int, list[str]]:
    """Run unity build structure check (formerly standalone test_unity_build.py).

    Returns:
        (violation_count, violation_messages)
    """
    result = check_unity_build()
    return (len(result.violations), result.violations)


def _convert_violations_to_results(violations: Any) -> CheckerResults:
    """Convert checker violations (dict or CheckerResults) to CheckerResults format.

    Args:
        violations: Either a CheckerResults object or legacy dict format

    Returns:
        CheckerResults object
    """
    if isinstance(violations, CheckerResults):
        return violations

    # Legacy dict format conversion
    results = CheckerResults()
    for file_path, violation_list in violations.items():
        if isinstance(violation_list, list):
            for item in violation_list:  # type: ignore[reportUnknownVariableType]
                if isinstance(item, tuple) and len(item) >= 2:  # type: ignore[reportUnknownArgumentType]
                    # Type narrowing: item is a tuple with at least 2 elements
                    line_num_raw, content_raw = item[0], item[1]  # type: ignore[reportUnknownVariableType]
                    line_num = int(line_num_raw)  # type: ignore[reportUnknownArgumentType]
                    content = str(content_raw)  # type: ignore[reportUnknownArgumentType]
                    results.add_violation(file_path, line_num, content)
        elif isinstance(violation_list, str):
            results.add_violation(file_path, 0, violation_list)

    return results


def _collect_violations_from_checkers(
    checkers: list[FileContentChecker],
) -> dict[str, CheckerResults]:
    """Collect violations from a list of checkers.

    Args:
        checkers: List of checker instances to collect violations from

    Returns:
        Dictionary mapping checker class name to CheckerResults
    """
    all_results: dict[str, CheckerResults] = {}

    for checker in checkers:
        checker_name = checker.__class__.__name__
        violations = getattr(checker, "violations", None)
        if not violations:
            continue

        results = _convert_violations_to_results(violations)

        # Merge violations from multiple checkers with same name
        if checker_name not in all_results:
            all_results[checker_name] = results
        else:
            # Merge into existing results
            for file_path, file_violations in results.violations.items():
                for violation in file_violations.violations:
                    all_results[checker_name].add_violation(
                        file_path, violation.line_number, violation.content
                    )

    return all_results


def _determine_file_scopes(file_path: str) -> set[str]:
    """Determine which checker scopes apply to a file.

    Args:
        file_path: Absolute path to the file

    Returns:
        Set of scope names that apply to this file
    """
    normalized_path = str(Path(file_path).resolve())
    posix_path = normalized_path.replace("\\", "/")
    scopes: set[str] = set()

    # Determine file characteristics
    is_src = "/src/" in posix_path
    is_example = "/examples/" in posix_path
    is_test = "/tests/" in posix_path
    is_fl = "/src/fl/" in posix_path
    is_lib8tion = "/src/lib8tion/" in posix_path
    is_fx = "/src/fx/" in posix_path
    is_sensors = "/src/sensors/" in posix_path
    is_platforms = "/src/platforms/" in posix_path
    is_platforms_shared = "/src/platforms/shared/" in posix_path
    is_third_party = "/src/third_party/" in posix_path

    # Add scopes based on file location
    if is_src or is_example or is_test:
        scopes.add("global")

    if is_src:
        scopes.add("src")
        scopes.add("native_platform_defines")

    if is_platforms:
        scopes.add("platforms")
        scopes.add("platforms_banned")

    if is_example:
        scopes.add("examples")
        scopes.add("examples_banned")

    if is_fl:
        scopes.add("fl")
        scopes.add("fl_platforms_include_paths")
        if normalized_path.endswith((".h", ".hpp", ".hxx", ".hh")):
            scopes.add("fl_headers")

    if is_lib8tion:
        scopes.add("lib8tion")

    if is_fx or is_sensors or is_platforms_shared:
        scopes.add("fx_sensors_platforms_shared")

    if is_platforms:
        scopes.add("fl_platforms_include_paths")

    if is_third_party:
        scopes.add("third_party")

    if is_test:
        scopes.add("tests")

    # Check for src root files
    if is_src and Path(normalized_path).parent.resolve() == SRC_ROOT.resolve():
        scopes.add("src_root")

    return scopes


def _collect_all_headers(files_by_dir: dict[str, list[str]]) -> frozenset[str]:
    """Collect all header files from the project for cross-file validation.

    Args:
        files_by_dir: Dictionary of files organized by directory

    Returns:
        Immutable frozenset of all header file paths
    """
    all_headers: set[str] = set()

    # Collect all header files from all directories
    for file_list in files_by_dir.values():
        for file_path in file_list:
            if file_path.endswith((".h", ".hpp", ".hh", ".hxx")):
                all_headers.add(file_path)

    return frozenset(all_headers)


def run_checkers_on_single_file(
    file_path: str, checkers_by_scope: dict[str, list[FileContentChecker]]
) -> dict[str, CheckerResults]:
    """Run all applicable checkers on a single file.

    Args:
        file_path: Absolute path to the file to check
        checkers_by_scope: Dictionary of checkers organized by scope

    Returns:
        Dictionary mapping checker class name to CheckerResults
    """
    processor = MultiCheckerFileProcessor()
    normalized_path = str(Path(file_path).resolve())

    # Determine which scopes apply to this file
    applicable_scopes = _determine_file_scopes(normalized_path)

    # Collect all checkers for the applicable scopes
    applicable_checkers: list[FileContentChecker] = []
    for scope in applicable_scopes:
        applicable_checkers.extend(checkers_by_scope.get(scope, []))

    # Remove duplicate checkers (same instance may be added from multiple scopes)
    seen_checkers: set[int] = set()
    unique_checkers: list[FileContentChecker] = []
    for checker in applicable_checkers:
        checker_id = id(checker)
        if checker_id not in seen_checkers:
            seen_checkers.add(checker_id)
            unique_checkers.append(checker)

    # Run all applicable checkers on this single file
    if unique_checkers:
        processor.process_files_with_checkers([normalized_path], unique_checkers)
        return _collect_violations_from_checkers(unique_checkers)

    return {}


def main() -> int:
    """Main entry point for unified C++ linting."""
    import argparse

    parser = argparse.ArgumentParser(
        description="Run unified C++ linting checks on all files or a single file"
    )
    parser.add_argument(
        "file", nargs="?", help="Optional: single file to check (faster)"
    )
    args = parser.parse_args()

    if args.file:
        # Single file mode
        file_path = Path(args.file).resolve()
        if not file_path.exists():
            print(f"Error: File not found: {file_path}")
            return 1

        print(f"Running unified C++ linting checks on single file...")
        print(f"File: {file_path}")
        print()

        # Collect all headers for cross-file validation (even in single-file mode)
        files_by_dir = collect_all_files_by_directory()
        all_headers = _collect_all_headers(files_by_dir)

        # Create all checker instances
        checkers_by_scope = create_checkers(all_headers=all_headers)

        # Run all applicable checkers on the single file
        results = run_checkers_on_single_file(str(file_path), checkers_by_scope)

        # Run targeted unity build check for .cpp.hpp and _build.cpp.hpp files
        if file_path.name.endswith(".cpp.hpp"):
            unity_result = check_unity_build_single_file(file_path)
            if not unity_result.success:
                unity_checker_results = CheckerResults()
                for violation in unity_result.violations:
                    unity_checker_results.add_violation(
                        "unity_build_structure", 0, violation
                    )
                results["UnityBuildChecker"] = unity_checker_results

        # Format and print results
        exit_code = format_and_print_results(results)

        return exit_code
    else:
        # Normal mode - check all files
        print("Running unified C++ linting checks...")
        print(f"Project root: {PROJECT_ROOT}")
        print()

        # Collect all files by directory
        files_by_dir = collect_all_files_by_directory()

        # Extract all headers for cross-file validation
        all_headers = _collect_all_headers(files_by_dir)

        # Create all checker instances
        checkers_by_scope = create_checkers(all_headers=all_headers)

        # Run all checkers in a single pass per scope
        results = run_checkers(files_by_dir, checkers_by_scope)

        # Run unity build structure check (formerly standalone subprocess)
        unity_violation_count, unity_violations = run_unity_build_check()
        if unity_violation_count > 0:
            unity_results = CheckerResults()
            for violation in unity_violations:
                unity_results.add_violation("unity_build_structure", 0, violation)
            results["UnityBuildChecker"] = unity_results

        # Format and print results
        exit_code = format_and_print_results(results)

        return exit_code


if __name__ == "__main__":
    sys.exit(main())
