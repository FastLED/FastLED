"""Argument parsing for lint.py."""

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class LintArgs:
    """Parsed command-line arguments for linting."""

    js_only: bool = False
    cpp_only: bool = False
    no_fingerprint: bool = False
    run_full: bool = False
    run_iwyu: bool = False
    run_pyright: bool = False
    run_tidy: bool = False
    files: list[str] = field(default_factory=lambda: list[str]())


def parse_lint_args(argv: list[str] | None = None) -> LintArgs:
    """
    Parse command-line arguments for linting.

    Args:
        argv: Command-line arguments (defaults to sys.argv[1:])

    Returns:
        LintArgs object with parsed arguments
    """
    parser = argparse.ArgumentParser(
        description="FastLED Comprehensive Linting Suite",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
This script runs comprehensive linting for Python, C++, and JavaScript.

Python linting includes:
  - ruff (lint + format) + KBI checker + ty
  - Use --strict to also run pyright (strict type checking, adds ~12s)

C++ linting includes:
  - clang-format and custom checkers
  - IWYU (Include-What-You-Use) runs with --full, --iwyu, or --strict
  - clang-tidy static analysis runs with --tidy

JavaScript linting:
  - FAST ONLY (skips if not available)

Examples:
  bash lint                    # Run all linters (Python, C++, JavaScript)
  bash lint --strict           # Also run pyright + IWYU on single files (strict mode)
  bash lint --js               # JavaScript linting only
  bash lint --cpp              # C++ linting only
  bash lint --full             # Include IWYU (Include-What-You-Use) on all files
  bash lint --iwyu             # Run IWYU only
  bash lint --tidy             # Run clang-tidy static analysis
  bash lint --no-fingerprint   # Force re-run all linters
  bash lint --strict file.cpp  # Single file with IWYU enabled
""",
    )

    parser.add_argument(
        "--js",
        action="store_true",
        help="Run JavaScript linting only",
    )

    parser.add_argument(
        "--cpp",
        action="store_true",
        help="Run C++ linting only",
    )

    parser.add_argument(
        "--no-fingerprint",
        action="store_true",
        help="Force re-run all linters (skip timestamp checks)",
    )

    parser.add_argument(
        "--full",
        action="store_true",
        help="Run all linters including IWYU (Include-What-You-Use)",
    )

    parser.add_argument(
        "--iwyu",
        action="store_true",
        help="Run IWYU (Include-What-You-Use) analysis only",
    )

    parser.add_argument(
        "--strict",
        action="store_true",
        help="Also run pyright on Python files + IWYU on C++ files (strict type & include checking)",
    )

    parser.add_argument(
        "--tidy",
        action="store_true",
        help="Run clang-tidy static analysis on C++ files",
    )

    parser.add_argument(
        "files",
        nargs="*",
        default=[],
        help="Optional: specific file(s) to lint (auto-detects type by extension)",
    )

    if argv is None:
        argv = sys.argv[1:]

    args = parser.parse_args(argv)

    files = [str(Path(f).resolve()) for f in args.files]

    # Single-file mode must never use fingerprint cache — always lint the file
    no_fingerprint = args.no_fingerprint or bool(files)

    return LintArgs(
        js_only=args.js,
        cpp_only=args.cpp,
        no_fingerprint=no_fingerprint,
        run_full=args.full,
        run_iwyu=args.iwyu,
        run_pyright=args.strict,
        run_tidy=args.tidy,
        files=files,
    )
