#!/usr/bin/env python3
import argparse
import os
import sys
from pathlib import Path
from typing import Optional

from typeguard import typechecked

from ci.util.test_types import TestArgs


def _python_test_exists(test_name: str) -> bool:
    """Check if a Python test file exists for the given test name"""
    # Check for the test file in ci/tests directory
    tests_dir = Path("ci/tests")

    # Try various naming patterns for Python tests
    possible_names = [
        f"test_{test_name}.py",
        f"{test_name}.py",
    ]

    for name in possible_names:
        test_file = tests_dir / name
        if test_file.exists():
            return True

    return False


def parse_args(args: Optional[list[str]] = None) -> TestArgs:
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(description="Run FastLED tests")
    parser.add_argument(
        "--cpp",
        action="store_true",
        help="Run C++ tests only (equivalent to --unit --examples, suppresses Python tests)",
    )
    parser.add_argument("--unit", action="store_true", help="Run C++ unit tests only")
    parser.add_argument("--py", action="store_true", help="Run Python tests only")
    parser.add_argument(
        "test",
        type=str,
        nargs="?",
        default=None,
        help="Specific test to run (Python or C++)",
    )

    # Create mutually exclusive group for compiler selection
    compiler_group = parser.add_mutually_exclusive_group()
    compiler_group.add_argument(
        "--clang", action="store_true", help="Use Clang compiler"
    )
    compiler_group.add_argument(
        "--gcc", action="store_true", help="Use GCC compiler (default on non-Windows)"
    )

    parser.add_argument(
        "--clean", action="store_true", help="Clean build before compiling"
    )
    parser.add_argument(
        "--no-interactive",
        action="store_true",
        help="Force non-interactive mode (no confirmation prompts)",
    )
    parser.add_argument(
        "--interactive",
        action="store_true",
        help="Enable interactive mode (allows confirmation prompts)",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Enable verbose output showing all test details",
    )
    parser.add_argument(
        "--show-compile",
        action="store_true",
        help="Show compilation commands and output",
    )
    parser.add_argument(
        "--show-link",
        action="store_true",
        help="Show linking commands and output",
    )
    parser.add_argument(
        "--quick", action="store_true", help="Enable quick mode with FASTLED_ALL_SRC=1"
    )
    parser.add_argument(
        "--no-stack-trace",
        action="store_true",
        help="Disable stack trace dumping on timeout",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Enable static analysis (IWYU, clang-tidy) - auto-enables --cpp and --clang",
    )
    parser.add_argument(
        "--examples",
        nargs="*",
        help="Run example compilation tests only (optionally specify example names). Use with --full for complete compilation + linking + execution",
    )
    parser.add_argument(
        "--no-pch",
        action="store_true",
        help="Disable precompiled headers (PCH) when running example compilation tests",
    )
    parser.add_argument(
        "--unity",
        action="store_true",
        help="Enable UNITY build mode for examples - compile all source files as a single unit for improved performance",
    )
    parser.add_argument(
        "--no-unity",
        action="store_true",
        help="Disable unity builds for cpp tests and examples",
    )
    parser.add_argument(
        "--full",
        action="store_true",
        help="Run full integration tests including compilation + linking + program execution",
    )

    parser.add_argument(
        "--no-parallel",
        action="store_true",
        help="Force sequential test execution",
    )
    parser.add_argument(
        "--unity-chunks",
        type=int,
        default=1,
        help="Number of unity chunks when building libfastled.a (default: 1)",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="Enable debug mode for C++ unit tests (e.g., full debug symbols)",
    )
    parser.add_argument(
        "--qemu",
        nargs="+",
        help="Run examples in QEMU emulation. Usage: --qemu esp32s3 [example_names...]",
    )

    parsed_args = parser.parse_args(args)

    # Convert argparse.Namespace to TestArgs dataclass
    test_args = TestArgs(
        cpp=parsed_args.cpp,
        unit=parsed_args.unit,
        py=parsed_args.py,
        test=parsed_args.test,
        clang=parsed_args.clang,
        gcc=parsed_args.gcc,
        clean=parsed_args.clean,
        no_interactive=parsed_args.no_interactive,
        interactive=parsed_args.interactive,
        verbose=parsed_args.verbose,
        show_compile=parsed_args.show_compile,
        show_link=parsed_args.show_link,
        quick=parsed_args.quick,
        no_stack_trace=parsed_args.no_stack_trace,
        check=parsed_args.check,
        examples=parsed_args.examples,
        no_pch=parsed_args.no_pch,
        unity=parsed_args.unity,
        no_unity=parsed_args.no_unity,
        full=parsed_args.full,
        no_parallel=parsed_args.no_parallel,
        unity_chunks=parsed_args.unity_chunks,
        debug=parsed_args.debug,
        qemu=parsed_args.qemu,
    )

    # Auto-enable --py or --cpp mode when a specific test is provided
    if test_args.test:
        # Check if this is a Python test first
        if _python_test_exists(test_args.test):
            # This is a Python test - enable Python mode
            if not test_args.py and not test_args.cpp:
                test_args.py = True
                print(
                    f"Auto-enabled --py mode for specific Python test: {test_args.test}"
                )
        else:
            # This is not a Python test - assume it's a C++ test
            if not test_args.cpp and not test_args.py:
                test_args.cpp = True
                print(f"Auto-enabled --cpp mode for specific test: {test_args.test}")
            # Also enable --unit when a specific C++ test is provided without any other flags
            if (
                not test_args.unit
                and not test_args.examples
                and not test_args.py
                and not test_args.full
            ):
                test_args.unit = True
                print(f"Auto-enabled --unit mode for specific test: {test_args.test}")

    # Auto-enable --verbose when running unit tests (disabled)
    # if test_args.unit and not test_args.verbose:
    #     test_args.verbose = True
    #     print("Auto-enabled --verbose mode for unit tests")

    # Auto-enable --cpp and --clang when --check is provided
    if test_args.check:
        if not test_args.cpp:
            test_args.cpp = True
            print("Auto-enabled --cpp mode for static analysis (--check)")
        if not test_args.clang and not test_args.gcc:
            test_args.clang = True
            print("Auto-enabled --clang compiler for static analysis (--check)")

    # Auto-enable --cpp and --quick when --examples is provided
    if test_args.examples is not None:
        if not test_args.cpp:
            test_args.cpp = True
            print("Auto-enabled --cpp mode for example compilation (--examples)")
        if not test_args.quick:
            test_args.quick = True
            print(
                "Auto-enabled --quick mode for faster example compilation (--examples)"
            )

    # Handle --full flag behavior
    if test_args.full:
        if test_args.examples is not None:
            # --examples --full: Run examples with full compilation+linking+execution
            print("Full examples mode: compilation + linking + program execution")
        else:
            # --full alone: Run integration tests
            if not test_args.cpp:
                test_args.cpp = True
                print("Auto-enabled --cpp mode for full integration tests (--full)")
            print("Full integration tests: compilation + linking + program execution")

    # Default to Clang on Windows unless --gcc is explicitly passed
    if sys.platform == "win32" and not test_args.gcc and not test_args.clang:
        test_args.clang = True
        print("Windows detected: defaulting to Clang compiler (use --gcc to override)")
    elif test_args.gcc:
        print("Using GCC compiler")
    elif test_args.clang:
        print("Using Clang compiler")

    # Validate conflicting arguments
    if test_args.no_interactive and test_args.interactive:
        print(
            "Error: --interactive and --no-interactive cannot be used together",
            file=sys.stderr,
        )
        sys.exit(1)

    # Set NO_PARALLEL environment variable if --no-parallel is used
    if test_args.no_parallel:
        os.environ["NO_PARALLEL"] = "1"
        print("Forcing sequential execution (NO_PARALLEL=1)")

    return test_args
