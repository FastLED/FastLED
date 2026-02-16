#!/usr/bin/env python3
"""
FastLED Comprehensive Linting Suite (Python Implementation)

Replaces the bash ./lint script with a Python implementation
following the trampoline pattern.
"""

import _thread  # noqa: F401 - Required for KBI linter
import os
import sys
from pathlib import Path

from ci.lint.args_parser import LintArgs, parse_lint_args
from ci.lint.duration_tracker import DurationTracker
from ci.lint.orchestrator import LintOrchestrator
from ci.lint.stage_impls import (
    run_clang_tidy,
    run_cpp_lint,
    run_cpp_lint_single_file,
    run_iwyu_analysis,
    run_iwyu_pragma_check,
    run_js_lint,
    run_js_lint_single_file,
    run_python_lint_single_file,
    run_python_pipeline,
)
from ci.lint.stages import LintStage
from ci.util.global_interrupt_handler import install_signal_handler, wait_for_cleanup


# File extension mappings for single-file mode
CPP_EXTENSIONS = {".cpp", ".h", ".hpp", ".hxx", ".hh", ".ino"}
PYTHON_EXTENSIONS = {".py"}
JS_EXTENSIONS = {".js", ".ts"}


def run_single_file_mode(files: list[str], strict: bool = False) -> bool:
    """Run linting on specific files, auto-detecting type by extension.

    Args:
        files: List of absolute file paths to lint
        strict: If True, run pyright on Python files and IWYU on C++ files
    """
    success = True
    for file_path in files:
        ext = Path(file_path).suffix.lower()
        if ext in CPP_EXTENSIONS:
            if not run_cpp_lint_single_file(file_path, strict=strict):
                success = False
        elif ext in PYTHON_EXTENSIONS:
            if not run_python_lint_single_file(file_path, strict=strict):
                success = False
        elif ext in JS_EXTENSIONS:
            if not run_js_lint_single_file(file_path):
                success = False
        else:
            print(f"⚠️  Unknown file type: {file_path} (skipping)")
    return success


def cleanup_visual_studio_files() -> None:
    """Remove Visual Studio project files if they exist."""
    tests_dir = Path("tests")
    if tests_dir.exists():
        for pattern in ["*.vcxproj", "*.vcxproj.filters", "*.sln"]:
            for file_path in tests_dir.glob(f"**/{pattern}"):
                try:
                    file_path.unlink()
                except KeyboardInterrupt:
                    _thread.interrupt_main()
                    raise
                except Exception:
                    pass  # Ignore errors


def print_header(js_only: bool, cpp_only: bool) -> None:
    """Print the header based on mode."""
    if js_only:
        print("🌐 Running JavaScript Linting Only")
        print("===================================")
    elif cpp_only:
        print("🔧 Running C++ Linting Only")
        print("============================")
    else:
        print("🚀 Running FastLED Comprehensive Linting Suite")
        print("==============================================")


def print_footer(js_only: bool, cpp_only: bool) -> None:
    """Print the completion message."""
    print("")
    if js_only:
        print("🎉 JavaScript linting completed!")
    elif cpp_only:
        print("🎉 C++ linting completed!")
    else:
        print("🎉 All linting completed!")


def print_ai_hints() -> None:
    """Print AI agent hints."""
    print("")
    print("💡 FOR AI AGENTS:")
    print("  - Use 'bash lint' for comprehensive linting (Python, C++, and JavaScript)")
    print(
        "  - Use 'bash lint --strict' to also run pyright + IWYU (strict type & include checking)"
    )
    print("  - Use 'bash lint --js' for JavaScript linting only")
    print("  - Use 'bash lint --cpp' for C++ linting only")
    print("  - Use 'bash lint --full' to include IWYU (Include-What-You-Use) analysis")
    print("  - Use 'bash lint --iwyu' to run IWYU analysis only")
    print("  - Use 'bash lint --tidy' to run clang-tidy static analysis on src/fl")
    print("  - Python linting includes: ruff (lint + format) + KBI checker + ty")
    print("  - Use --strict to also run pyright (strict type checking)")
    print("  - C++ linting includes: clang-format and custom checkers")
    print("  - IWYU runs with --full, --iwyu, or --strict flags")
    print("  - clang-tidy runs with --tidy flag")
    print("  - JavaScript linting: FAST ONLY (no slow fallback)")
    print("  - To enable fast JavaScript linting: uv run ci/setup-js-linting-fast.py")
    print("  - Use 'bash lint --help' for usage information")


def create_stages(args: LintArgs) -> list[LintStage]:
    """Create lint stages based on arguments."""
    stages: list[LintStage] = []

    # Determine which stages to run
    run_python = not args.js_only and not args.cpp_only
    run_cpp = not args.js_only
    run_js = not args.cpp_only

    # Python pipeline (ruff -> ty -> pyright)
    # Run as a single stage to match bash behavior
    if run_python:
        stages.append(
            LintStage(
                name="python_pipeline",
                display_name="PYTHON PIPELINE",
                run_fn=lambda: run_python_pipeline(
                    args.no_fingerprint, args.run_pyright
                ),
                timeout=360.0,  # Combined timeout for all Python stages
            )
        )

    # C++ stage (includes IWYU check within the same stage)
    if run_cpp:

        def run_cpp_and_iwyu() -> bool:
            """Run C++ linting and IWYU analysis together."""
            if not run_cpp_lint(args.no_fingerprint, args.run_full, args.run_iwyu):
                return False
            if not run_iwyu_pragma_check():
                return False
            if not run_iwyu_analysis(args.run_full, args.run_iwyu, args.run_pyright):
                return False
            if not run_clang_tidy(args.no_fingerprint, args.run_tidy):
                return False
            return True

        stages.append(
            LintStage(
                name="cpp_linting",
                display_name="C++ LINTING",
                run_fn=run_cpp_and_iwyu,
                timeout=600.0,  # Combined timeout for C++ + IWYU + clang-tidy
            )
        )

    # JavaScript stage
    if run_js:
        stages.append(
            LintStage(
                name="javascript_linting",
                display_name="JAVASCRIPT LINTING",
                run_fn=lambda: run_js_lint(args.no_fingerprint),
                timeout=120.0,
            )
        )

    return stages


def main() -> int:
    """Main entry point for lint.py."""
    # Parse arguments
    args = parse_lint_args()

    # Unset VIRTUAL_ENV to avoid warnings
    os.environ.pop("VIRTUAL_ENV", None)

    # Single-file mode
    if args.files:
        print(f"🔍 Single-file lint mode ({len(args.files)} file(s))")
        print("=" * 50)
        install_signal_handler()
        success = run_single_file_mode(args.files, strict=args.run_pyright)
        return 0 if success else 1

    # Cleanup Visual Studio files
    cleanup_visual_studio_files()

    # Print header
    print_header(args.js_only, args.cpp_only)

    # Install interrupt handler
    install_signal_handler()

    try:
        # Create stages
        stages = create_stages(args)

        # Create duration tracker
        tracker = DurationTracker()

        # Determine if we should run in parallel
        # Parallel mode: 2+ stages and not in single-mode (--cpp/--js)
        parallel = len(stages) > 1 and not args.js_only and not args.cpp_only

        # Create orchestrator
        orchestrator = LintOrchestrator(stages, tracker, parallel)

        # Run all stages
        success = orchestrator.run()

        # Print footer
        print_footer(args.js_only, args.cpp_only)

        # Print summary table
        print(tracker.generate_summary())

        # Print AI hints
        print_ai_hints()

        # Return exit code
        return 0 if success else 1

    except KeyboardInterrupt:
        print("\n⚠️  Interrupted by user")
        _thread.interrupt_main()
        wait_for_cleanup()
        return 130


if __name__ == "__main__":
    sys.exit(main())
