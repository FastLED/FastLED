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

from ci.lint_cpp.banned_headers_checker import (
    BANNED_HEADERS_COMMON,
    BANNED_HEADERS_CORE,
    BANNED_HEADERS_PLATFORMS,
    BannedHeadersChecker,
)

# Import all checker classes
from ci.lint_cpp.check_namespace_includes import NamespaceIncludesChecker
from ci.lint_cpp.check_platforms_fl_namespace import PlatformsFlNamespaceChecker
from ci.lint_cpp.check_using_namespace import UsingNamespaceChecker
from ci.lint_cpp.cpp_include_checker import CppIncludeChecker
from ci.lint_cpp.google_member_style_checker import GoogleMemberStyleChecker
from ci.lint_cpp.headers_exist_checker import HeadersExistChecker
from ci.lint_cpp.include_after_namespace_checker import IncludeAfterNamespaceChecker
from ci.lint_cpp.include_paths_checker import IncludePathsChecker
from ci.lint_cpp.logging_in_iram_checker import LoggingInIramChecker
from ci.lint_cpp.no_namespace_fl_declaration import NamespaceFlDeclarationChecker
from ci.lint_cpp.no_using_namespace_fl_in_headers import UsingNamespaceFlChecker
from ci.lint_cpp.numeric_limit_macros_checker import NumericLimitMacroChecker
from ci.lint_cpp.platform_includes_checker import PlatformIncludesChecker
from ci.lint_cpp.reinterpret_cast_checker import ReinterpretCastChecker
from ci.lint_cpp.serial_printf_checker import SerialPrintfChecker
from ci.lint_cpp.static_in_headers_checker import StaticInHeaderChecker
from ci.lint_cpp.std_namespace_checker import StdNamespaceChecker
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


def create_checkers() -> dict[str, list[FileContentChecker]]:
    """Create all checker instances organized by which files they should check."""
    checkers_by_scope: dict[str, list[FileContentChecker]] = {}

    # Global checkers (run on all src/, examples/, tests/ files)
    checkers_by_scope["global"] = [
        CppIncludeChecker(),
        IncludeAfterNamespaceChecker(),
        GoogleMemberStyleChecker(),
        NumericLimitMacroChecker(),
        StaticInHeaderChecker(),
        LoggingInIramChecker(),
        PlatformIncludesChecker(),
        # Note: Private libc++ headers checking is now integrated into BannedHeadersChecker
    ]

    # Src-only checkers
    checkers_by_scope["src"] = [
        UsingNamespaceFlChecker(),
        StdNamespaceChecker(),
        NamespaceIncludesChecker(),
        ReinterpretCastChecker(),
    ]

    # Platforms-specific checkers
    checkers_by_scope["platforms"] = [
        PlatformsFlNamespaceChecker(),
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
    ]

    # fl/ directory checkers with STRICT enforcement
    checkers_by_scope["fl"] = [
        BannedHeadersChecker(banned_headers_list=BANNED_HEADERS_CORE, strict_mode=True),
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

    for checker in all_checkers:
        checker_name = checker.__class__.__name__
        violations = getattr(checker, "violations", None)
        if violations:
            # Convert legacy dict format to CheckerResults
            if isinstance(violations, CheckerResults):
                results = violations
            else:
                # Legacy dict format
                results = CheckerResults()
                for file_path, violation_list in violations.items():
                    if isinstance(violation_list, list):
                        for item in violation_list:  # type: ignore[misc]
                            if isinstance(item, tuple) and len(item) >= 2:  # type: ignore[arg-type]
                                line_num: int = int(item[0])  # type: ignore[misc]
                                content: str = str(item[1])  # type: ignore[misc]
                                results.add_violation(file_path, line_num, content)
                    elif isinstance(violation_list, str):
                        results.add_violation(file_path, 0, violation_list)

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


def main() -> int:
    """Main entry point for unified C++ linting."""
    print("Running unified C++ linting checks...")
    print(f"Project root: {PROJECT_ROOT}")
    print()

    # Collect all files by directory
    files_by_dir = collect_all_files_by_directory()

    # Create all checker instances
    checkers_by_scope = create_checkers()

    # Run all checkers in a single pass per scope
    results = run_checkers(files_by_dir, checkers_by_scope)

    # Format and print results
    exit_code = format_and_print_results(results)

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
