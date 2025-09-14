#!/usr/bin/env python3
import hashlib
import json
import time
from dataclasses import dataclass
from enum import Enum, auto
from pathlib import Path
from typing import Any, Dict, List, Optional

from typeguard import typechecked


class TestResultType(Enum):
    """Type of test result message"""

    SUCCESS = auto()
    ERROR = auto()
    WARNING = auto()
    INFO = auto()
    DEBUG = auto()


@typechecked
@dataclass
class TestResult:
    """Structured test result"""

    type: TestResultType
    message: str
    test_name: Optional[str] = None
    details: Optional[Dict[str, Any]] = None
    timestamp: float = 0.0

    def __post_init__(self):
        if not self.timestamp:
            self.timestamp = time.time()


@typechecked
@dataclass
class TestSuiteResult:
    """Results for a test suite"""

    name: str
    results: List[TestResult]
    start_time: float
    end_time: Optional[float] = None
    passed: bool = True

    @property
    def duration(self) -> float:
        """Get test duration in seconds"""
        if self.end_time is None:
            return 0.0
        return self.end_time - self.start_time


@typechecked
@dataclass
class TestArgs:
    """Type-safe test arguments"""

    cpp: bool = False
    unit: bool = False
    py: bool = False
    test: Optional[str] = None
    clang: bool = False
    gcc: bool = False
    clean: bool = False
    no_interactive: bool = False
    interactive: bool = False
    verbose: bool = False
    show_compile: bool = False
    show_link: bool = False
    quick: bool = False
    no_stack_trace: bool = False
    check: bool = False
    examples: Optional[list[str]] = None
    no_pch: bool = False
    unity: bool = False
    no_unity: bool = False  # Disable unity builds for cpp tests and examples
    full: bool = False

    no_parallel: bool = False  # Force sequential test execution
    unity_chunks: int = 1  # Number of unity chunks for libfastled build
    debug: bool = False  # Enable debug mode for unit tests
    qemu: Optional[list[str]] = None  # Run examples in QEMU emulation


@typechecked
@dataclass
class TestCategories:
    """Type-safe test category flags"""

    unit: bool
    examples: bool
    py: bool
    integration: bool
    unit_only: bool
    examples_only: bool
    py_only: bool
    integration_only: bool
    qemu_esp32s3: bool
    qemu_esp32s3_only: bool

    def __post_init__(self):
        # Type validation
        for field_name in [
            "unit",
            "examples",
            "py",
            "integration",
            "unit_only",
            "examples_only",
            "py_only",
            "integration_only",
            "qemu_esp32s3",
            "qemu_esp32s3_only",
        ]:
            value = getattr(self, field_name)
            if not isinstance(value, bool):
                raise TypeError(f"{field_name} must be bool, got {type(value)}")


@typechecked
@dataclass
class FingerprintResult:
    """Type-safe fingerprint result"""

    hash: str
    elapsed_seconds: Optional[str] = None
    status: Optional[str] = None


def process_test_flags(args: TestArgs) -> TestArgs:
    """Process and validate test execution flags"""

    # Check which specific test flags are provided
    specific_flags = [
        args.unit,
        args.examples is not None,
        args.py,
        args.full,
        args.qemu is not None,
    ]
    specific_count = sum(bool(flag) for flag in specific_flags)

    # If --cpp is provided, default to --unit --examples (no Python)
    if args.cpp and specific_count == 0:
        args.unit = True
        args.examples = []  # Empty list means run all examples
        print("--cpp mode: Running unit tests and examples (Python tests suppressed)")
        return args

    # If any specific flags are provided, ONLY run those (exclusive behavior)
    if specific_count > 0:
        # When specific flags are provided, disable everything else unless explicitly set
        if not args.unit:
            # Unit was not explicitly set, so disable it
            pass  # args.unit is already False

        # Auto-enable verbose mode for unit tests (disabled)
        # if args.unit and not args.verbose:
        #     args.verbose = True
        #     print("Auto-enabled --verbose mode for unit tests")
        if args.examples is None:
            # Examples was not explicitly set, so disable it
            pass  # args.examples is already None
        if not args.py:
            # Python was not explicitly set, so disable it
            pass  # args.py is already False
        if not args.full:
            # Full was not explicitly set, so disable it
            pass  # args.full is already False

        # Log what's running
        enabled_tests: list[str] = []
        if args.unit:
            enabled_tests.append("unit tests")
        if args.examples is not None:
            enabled_tests.append("examples")
        if args.py:
            enabled_tests.append("Python tests")
        if args.full:
            if args.examples is not None:
                enabled_tests.append("full example integration tests")
            else:
                enabled_tests.append("full integration tests")

        print(f"Specific test flags provided: Running only {', '.join(enabled_tests)}")
        return args

    # If no specific flags, run everything (backward compatibility)
    if specific_count == 0:
        args.unit = True
        args.examples = []  # Empty list means run all examples
        args.py = True
        print("No test flags specified: Running all tests (unit, examples, Python)")

        # Auto-enable verbose mode for unit tests (disabled)
        # if args.unit and not args.verbose:
        #     args.verbose = True
        #     print("Auto-enabled --verbose mode for unit tests")

        return args

    return args


def determine_test_categories(args: TestArgs) -> TestCategories:
    """Determine which test categories should run based on flags"""
    unit_enabled = args.unit
    examples_enabled = args.examples is not None
    py_enabled = args.py
    # Integration tests only run when --full is used alone (not with --examples)
    integration_enabled = args.full and args.examples is None
    qemu_esp32s3_enabled = args.qemu is not None

    return TestCategories(
        unit=unit_enabled,
        examples=examples_enabled,
        py=py_enabled,
        integration=integration_enabled,
        qemu_esp32s3=qemu_esp32s3_enabled,
        unit_only=unit_enabled
        and not examples_enabled
        and not py_enabled
        and not integration_enabled
        and not qemu_esp32s3_enabled,
        examples_only=examples_enabled
        and not unit_enabled
        and not py_enabled
        and not integration_enabled
        and not qemu_esp32s3_enabled,
        py_only=py_enabled
        and not unit_enabled
        and not examples_enabled
        and not integration_enabled
        and not qemu_esp32s3_enabled,
        integration_only=integration_enabled
        and not unit_enabled
        and not examples_enabled
        and not py_enabled
        and not qemu_esp32s3_enabled,
        qemu_esp32s3_only=qemu_esp32s3_enabled
        and not unit_enabled
        and not examples_enabled
        and not py_enabled
        and not integration_enabled,
    )


def fingerprint_code_base(
    start_directory: Path, glob: str = "**/*.h,**/*.cpp,**/*.hpp"
) -> FingerprintResult:
    """
    Create a fingerprint of the code base by hashing file contents.

    Args:
        start_directory: The root directory to start scanning from
        glob: Comma-separated list of glob patterns to match files

    Returns:
        A FingerprintResult with hash and optional status
    """
    try:
        hasher = hashlib.sha256()
        patterns = glob.split(",")

        # Get all matching files
        all_files: list[Path] = []
        for pattern in patterns:
            pattern = pattern.strip()
            all_files.extend(sorted(start_directory.glob(pattern)))

        # Sort files for consistent ordering
        all_files.sort()

        # Process each file
        for file_path in all_files:
            if file_path.is_file():
                # Add the relative path to the hash
                rel_path = file_path.relative_to(start_directory)
                hasher.update(str(rel_path).encode("utf-8"))

                # Add the file content to the hash
                try:
                    with open(file_path, "rb") as f:
                        # Read in chunks to handle large files
                        for chunk in iter(lambda: f.read(4096), b""):
                            hasher.update(chunk)
                except Exception as e:
                    # If we can't read the file, include the error in the hash
                    hasher.update(f"ERROR:{str(e)}".encode("utf-8"))

        return FingerprintResult(hash=hasher.hexdigest())
    except Exception as e:
        return FingerprintResult(hash="", status=f"error: {str(e)}")


def calculate_fingerprint(root_dir: Path | None = None) -> FingerprintResult:
    """
    Calculate the code base fingerprint.

    Args:
        root_dir: The root directory to start scanning from. If None, uses src directory.

    Returns:
        The fingerprint result
    """
    if root_dir is None:
        root_dir = Path.cwd() / "src"

    start_time = time.time()
    # Compute the fingerprint
    result = fingerprint_code_base(root_dir)
    elapsed_time = time.time() - start_time
    # Add timing information to the result
    result.elapsed_seconds = f"{elapsed_time:.2f}"

    return result
