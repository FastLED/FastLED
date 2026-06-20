#!/usr/bin/env python3
"""
Unified C++ linter runner.

Loads all checker modules and runs them in a single file-loading pass.
This dramatically improves performance by loading each file only once
instead of K times (where K is the number of checkers).
"""

import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, cast

from ci.lint_cpp.rust_bridge import (
    merge_checker_results,
    run_rust_linter,
)
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
    # NOTE: The bulk of the legacy Python checker fleet was retired in favor
    # of the Rust C++ linter (see ``ci/lint_cpp/rust_bridge.py`` and
    # ``ci/lint_cpp_rs/``). Only the small set of Python-only checkers that
    # have no Rust port yet remain instantiated here. The ``all_headers``
    # argument is kept for API compatibility — it is no longer consumed by
    # any Python-side checker.
    del all_headers  # currently unused; kept for API compatibility

    checkers_by_scope: dict[str, list[FileContentChecker]] = {}

    # Global checkers — all retired to Rust (#3297). The list is kept as an
    # empty placeholder so the dispatch loop and the empty-list shortcut
    # in run_checkers() still work; a future port can land its first new
    # Python-only checker here without re-introducing the scope key.
    checkers_by_scope["global"] = []

    # Src-only checkers — all retired to Rust.
    checkers_by_scope["src"] = []

    # Platforms-specific checkers — all retired to Rust.
    checkers_by_scope["platforms"] = []

    # Native platform defines checker — retired to Rust.
    checkers_by_scope["native_platform_defines"] = []

    # Include paths checker (fl/ and platforms/) — retired to Rust.
    checkers_by_scope["fl_platforms_include_paths"] = []

    # Src root-only checkers — retired to Rust.
    checkers_by_scope["src_root"] = []

    # Examples-only checkers — retired to Rust.
    checkers_by_scope["examples"] = []

    # fl/ directory — retired to Rust (#3297). Empty placeholder kept so the
    # dispatch loop's empty-list shortcut works without scope-key churn.
    checkers_by_scope["fl"] = []

    # lib8tion/, fx_sensors_platforms_shared, platforms_banned, examples_banned,
    # third_party — all banned-header variants retired to Rust.
    checkers_by_scope["lib8tion"] = []
    checkers_by_scope["fx_sensors_platforms_shared"] = []
    checkers_by_scope["platforms_banned"] = []
    checkers_by_scope["examples_banned"] = []
    checkers_by_scope["third_party"] = []

    # fl/ header-only checker — retired to Rust.
    checkers_by_scope["fl_headers"] = []

    # Tests-only checkers — all retired to Rust.
    checkers_by_scope["tests"] = []

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


def format_and_print_results(
    results: dict[str, CheckerResults], max_violations_per_checker: int = 10
) -> int:
    """Format and print all violations. Returns exit code (0 = success, 1 = failures).

    Args:
        results: Checker results to format.
        max_violations_per_checker: Maximum violations to print per checker before
            truncating output. Set to 0 for unlimited.
    """
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

        printed_violations = 0
        truncated = False
        for file_path in sorted(checker_results.violations.keys()):
            if (
                max_violations_per_checker > 0
                and printed_violations >= max_violations_per_checker
            ):
                truncated = True
                break
            rel_path = os.path.relpath(file_path, PROJECT_ROOT)
            # Normalize to forward slashes for consistent cross-platform output
            rel_path = rel_path.replace("\\", "/")
            file_violations = checker_results.violations[file_path]

            print(f"\n{rel_path}:")
            for violation in file_violations.violations:
                if (
                    max_violations_per_checker > 0
                    and printed_violations >= max_violations_per_checker
                ):
                    truncated = True
                    break
                print(f"  Line {violation.line_number}: {violation.content}")
                printed_violations += 1

        if truncated:
            remaining = violation_count - printed_violations
            print(
                f"\n  ... stopped after {max_violations_per_checker} violations"
                f" ({remaining} more not shown)"
            )

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


def _ast_tool_unavailable(message: str) -> bool:
    """Return True iff *message* signals that the clang-query AST tool is
    absent from the runtime (vs. an actual lint violation).

    Captures both the "tool not found at resolve time" path inside
    ``_find_clang_query`` and the "uv resolved but couldn't spawn the
    inner binary" path that surfaces on CI runners without the full
    clang-tool-chain wheel installed.
    """
    lower = message.lower()
    tool_named = "clang-query" in lower or "clang-tool-chain-query" in lower
    return tool_named and ("not found" in lower or "failed to spawn" in lower)


def run_noexcept_ast_check(file_path: str | None = None) -> CheckerResults:
    """Run the clang-query FL_NO_EXCEPT ratchet used by default C++ lint."""
    from ci.tools.check_noexcept import (
        DEFAULT_BASELINE,
        NoexceptCheckError,
        diff_against_baseline,
        find_missing_noexcept,
        load_baseline,
    )

    scope = "all"
    rel_file: str | None = None
    if file_path is not None:
        rel_file = os.path.relpath(str(Path(file_path).resolve()), PROJECT_ROOT)
        rel_file = rel_file.replace("\\", "/")
        if rel_file.startswith("src/fl/"):
            scope = "fl"
        elif rel_file.startswith("src/platforms/"):
            scope = "platforms"
        elif rel_file.startswith("src/third_party/"):
            scope = "third_party"
        else:
            # Outside owned src scopes — nothing to check for this file.
            return CheckerResults()

    def _run() -> CheckerResults:
        results = CheckerResults()
        try:
            hits = find_missing_noexcept(scope)
        except NoexceptCheckError as exc:
            # If the underlying clang-query tool simply isn't on PATH (common on
            # CI runners without a full LLVM toolchain install), skip the AST
            # ratchet with a stderr warning rather than turning it into a hard
            # lint failure. The ratchet is one of the Tier-4 entries explicitly
            # out-of-scope per #3288 and is best effort outside dev boxes.
            # The two flavors we see in the wild:
            #   - "clang-query not found. Install LLVM or the clang-tool-chain ..."
            #     (raised by _find_clang_query returning empty)
            #   - "error: Failed to spawn: `clang-tool-chain-query` Caused by ..."
            #     (raised by uv when it can resolve uv itself but not the inner
            #     clang-tool-chain entry-point)
            message = str(exc)
            if _ast_tool_unavailable(message):
                print(
                    f"⚠️  Skipping FL_NO_EXCEPT AST ratchet — {message}", file=sys.stderr
                )
                return results
            results.add_violation("ci/tools/check_noexcept.py", 0, message)
            return results

        if rel_file is not None:
            hits = [hit for hit in hits if hit.path == rel_file]

        new_hits, _stale = diff_against_baseline(hits, load_baseline(DEFAULT_BASELINE))
        for hit in new_hits:
            abs_path = str(PROJECT_ROOT / hit.path)
            results.add_violation(
                abs_path, hit.line, f"Missing FL_NO_EXCEPT: {hit.line_text}"
            )
        return results

    # Single-file mode bypasses the cache - the fingerprint is built over
    # the whole scope and would cost more to compute than the per-file
    # check itself saves.
    if file_path is not None:
        return _run()

    from ci.lint_cpp.ast_cache import cached_ast_check

    return cached_ast_check(
        name="noexcept_ast",
        scope=scope,
        tool_sources=[PROJECT_ROOT / "ci" / "tools" / "check_noexcept.py"],
        baseline_path=PROJECT_ROOT / DEFAULT_BASELINE if DEFAULT_BASELINE else None,
        runner=_run,
    )


def run_array_param_ast_check(file_path: str | None = None) -> CheckerResults:
    """Run the clang-query decayed array parameter ratchet."""
    from ci.tools.check_array_params import (
        DEFAULT_BASELINE,
        ArrayParamCheckError,
        _diagnostic_for_hit,
        diff_against_baseline,
        find_decayed_array_params,
        load_baseline,
    )

    scope = "all"
    rel_file: str | None = None
    if file_path is not None:
        rel_file = os.path.relpath(str(Path(file_path).resolve()), PROJECT_ROOT)
        rel_file = rel_file.replace("\\", "/")
        if rel_file.startswith("src/fl/"):
            scope = "fl"
        elif rel_file.startswith("src/platforms/"):
            scope = "platforms"
        elif rel_file.startswith("src/third_party/"):
            scope = "third_party"
        else:
            return CheckerResults()

    def _run() -> CheckerResults:
        results = CheckerResults()
        try:
            hits = find_decayed_array_params(scope)
        except ArrayParamCheckError as exc:
            # Same fallback as run_noexcept_ast_check above — skip when the
            # underlying tool is missing rather than failing the whole lint.
            message = str(exc)
            if _ast_tool_unavailable(message):
                print(
                    f"⚠️  Skipping decayed-array-param AST ratchet — {message}",
                    file=sys.stderr,
                )
                return results
            results.add_violation("ci/tools/check_array_params.py", 0, message)
            return results

        if rel_file is not None:
            hits = [hit for hit in hits if hit.path == rel_file]

        new_hits, _stale = diff_against_baseline(hits, load_baseline(DEFAULT_BASELINE))
        for hit in new_hits:
            abs_path = str(PROJECT_ROOT / hit.path)
            results.add_violation(abs_path, hit.line, _diagnostic_for_hit(hit))
        return results

    if file_path is not None:
        return _run()

    from ci.lint_cpp.ast_cache import cached_ast_check

    return cached_ast_check(
        name="array_param_ast",
        scope=scope,
        tool_sources=[PROJECT_ROOT / "ci" / "tools" / "check_array_params.py"],
        baseline_path=PROJECT_ROOT / DEFAULT_BASELINE if DEFAULT_BASELINE else None,
        runner=_run,
    )


@dataclass(frozen=True)
class LegacyViolationLineContent:
    line_number: int
    message: str


def _legacy_violation_item_to_line_content(raw: Any) -> LegacyViolationLineContent:
    if isinstance(raw, tuple):
        tuple_item = cast(tuple[Any, ...], raw)
        if len(tuple_item) >= 2:
            line_num_raw: Any = tuple_item[0]
            content_raw: Any = tuple_item[1]
            return LegacyViolationLineContent(
                line_number=int(line_num_raw), message=str(content_raw)
            )
        return _build_line_content_from_attrs(tuple_item)

    return _build_line_content_from_attrs(raw)


def _build_line_content_from_attrs(item: Any) -> LegacyViolationLineContent:
    """Build a line-content record from a duck-typed legacy violation object."""
    include_line: Any = getattr(item, "include_line", None)
    include_snippet: Any = getattr(item, "include_snippet", None)
    if include_line is None or include_snippet is None:
        raise ValueError(
            f"Unsupported violation item shape: {item!r} ({type(item).__name__})"
        )

    message = str(include_snippet)
    namespace_info: Any = getattr(item, "namespace_info", None)
    if namespace_info is not None:
        namespace_line: Any = getattr(namespace_info, "line_number", None)
        namespace_snippet: Any = getattr(namespace_info, "snippet", None)
        if namespace_line is not None and namespace_snippet is not None:
            message = (
                f"{message} (namespace declared at line {int(namespace_line)}: "
                f"{namespace_snippet})"
            )
    return LegacyViolationLineContent(line_number=int(include_line), message=message)


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
                converted = _legacy_violation_item_to_line_content(item)
                results.add_violation(
                    file_path, converted.line_number, converted.message
                )
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
    parser.add_argument(
        "--rust",
        action=argparse.BooleanOptionalAction,
        default=True,
        help=(
            "Use the Rust linter for the checker subset it currently supports "
            "(default: on). Pass --no-rust to fall back to the Python-only path."
        ),
    )
    args = parser.parse_args()
    use_rust_fast_path = args.rust

    if args.file:
        # Single file mode
        file_path = Path(args.file).resolve()
        if not file_path.exists():
            print(f"Error: File not found: {file_path}")
            return 1

        print(f"Running unified C++ linting checks on single file...")
        print(f"File: {file_path}")
        print()

        # Collect files by directory so the file/scope dispatch below knows
        # which checkers to apply. Cross-file header collection used to feed
        # into create_checkers() but is no longer needed — every checker
        # that consumed it has been retired to the Rust binary.
        files_by_dir = collect_all_files_by_directory()

        # Create all checker instances
        checkers_by_scope = create_checkers()
        rust_results: dict[str, CheckerResults] = {}
        if use_rust_fast_path:
            rust_results = run_rust_linter([str(file_path)])

        # Run all applicable checkers on the single file
        results = run_checkers_on_single_file(str(file_path), checkers_by_scope)
        merge_checker_results(results, rust_results)

        # Per-file UnityBuildChecker now lives in the Rust crate
        # (ci/lint_cpp_rs/src/checkers/unity_build.rs); its violations
        # arrive via the merge_checker_results step above.

        # Per-file TestAggregationChecker now lives in the Rust crate
        # (ci/lint_cpp_rs/src/checkers/test_structure.rs); its violations
        # arrive via the merge_checker_results step above.

        noexcept_results = run_noexcept_ast_check(str(file_path))
        if noexcept_results.has_violations():
            results["NoexceptAstChecker"] = noexcept_results

        array_param_results = run_array_param_ast_check(str(file_path))
        if array_param_results.has_violations():
            results["ArrayParamAstChecker"] = array_param_results

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

        # Create all checker instances. The previous all_headers
        # cross-file collection has been retired with its consumers.
        checkers_by_scope = create_checkers()

        # Fan out the three independent heavy stages in parallel:
        #   - the Rust fastled-lint pass        (~5s, all-cores rayon-parallel)
        #   - the clang-query FL_NO_EXCEPT pass (~17s, single clang-query subprocess)
        #   - the clang-query decayed-array pass(~10s, single clang-query subprocess)
        # Wall time collapses from ~32s sequential to ~17s (clang-query bound).
        # The Python per-file run_checkers() pass is run on the foreground thread
        # since most of its scopes are currently empty buckets - kept separate so
        # any future heavy Python checker re-enables fast.
        from concurrent.futures import ThreadPoolExecutor

        with ThreadPoolExecutor(max_workers=3) as pool:
            rust_future = (
                pool.submit(run_rust_linter, None) if use_rust_fast_path else None
            )
            noexcept_future = pool.submit(run_noexcept_ast_check)
            array_param_future = pool.submit(run_array_param_ast_check)

            # Run the Python per-file pass on the foreground thread; the three
            # background subprocess-bound futures make progress in parallel.
            results = run_checkers(files_by_dir, checkers_by_scope)

            rust_results: dict[str, CheckerResults] = (
                rust_future.result() if rust_future is not None else {}
            )
            merge_checker_results(results, rust_results)

            # UnityBuildChecker (whole-project structural pass),
            # TestAggregationChecker (whole-project structural pass), and
            # PchFileChecker now ship in the Rust binary's run_structural_passes()
            # — see ci/lint_cpp_rs/src/checkers/structural_passes.rs. Violations
            # arrive via `merge_checker_results` above.

            noexcept_results = noexcept_future.result()
            if noexcept_results.has_violations():
                results["NoexceptAstChecker"] = noexcept_results

            array_param_results = array_param_future.result()
            if array_param_results.has_violations():
                results["ArrayParamAstChecker"] = array_param_results

        # Format and print results
        exit_code = format_and_print_results(results)

        return exit_code


if __name__ == "__main__":
    sys.exit(main())
