#!/usr/bin/env python3
import hashlib
import threading
import time
from dataclasses import dataclass
from enum import Enum, auto
from pathlib import Path
from typing import Any, Optional

from typeguard import typechecked

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


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
    details: Optional[dict[str, Any]] = None
    timestamp: float = 0.0

    def __post_init__(self):
        if not self.timestamp:
            self.timestamp = time.time()


@typechecked
@dataclass
class TestSuiteResult:
    """Results for a test suite"""

    name: str
    results: list[TestResult]
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
    stack_trace: bool = False
    check: bool = False
    examples: Optional[list[str]] = None
    no_pch: bool = False
    full: bool = False

    no_parallel: bool = False  # Force sequential test execution
    debug: bool = False  # Enable debug mode for unit tests and examples
    build_mode: Optional[str] = None  # Override build mode (quick, debug, release)
    qemu: Optional[list[str]] = (
        None  # Run examples in QEMU emulation (deprecated - use --run)
    )
    run: Optional[list[str]] = (
        None  # Run examples in emulation (QEMU for ESP32, avr8js for AVR)
    )
    no_fingerprint: bool = False  # Disable fingerprint caching
    build: bool = False  # Build Docker images if missing (use with --run)
    force: bool = False  # Force rerun of all tests, ignore fingerprint cache
    no_unity: bool = False  # Not in use any more, maybe revived later


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

    def should_skip(self, current: "FingerprintResult") -> bool:
        """
        Determine if we should skip running tests based on fingerprint comparison.

        Args:
            current: The current fingerprint calculated from the codebase

        Returns:
            True if we should skip (cache hit), False if we should run tests

        Logic:
            - Skip if hash matches AND previous status was "success"
            - Always run if hash differs (code changed)
            - Always run if previous status was not "success" (previous failure)
        """
        if self.hash != current.hash:
            # Code changed - must run
            return False
        if self.status != "success":
            # Previous run failed - must run again
            return False
        # Hash matches and previous run succeeded - safe to skip
        return True


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

        # Log what's running (disabled - redundant with auto-enabled messages)
        # enabled_tests: list[str] = []
        # if args.unit:
        #     enabled_tests.append("unit tests")
        # if args.examples is not None:
        #     enabled_tests.append("examples")
        # if args.py:
        #     enabled_tests.append("Python tests")
        # if args.full:
        #     if args.examples is not None:
        #         enabled_tests.append("full example integration tests")
        #     else:
        #         enabled_tests.append("full integration tests")
        # print(f"Specific test flags provided: Running only {', '.join(enabled_tests)}")
        return args

    # If no specific flags, run everything (backward compatibility)
    if specific_count == 0:
        args.unit = True
        args.examples = []  # Empty list means run all examples
        args.py = True
        # print("No test flags specified: Running all tests (unit, examples, Python)")

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
                except KeyboardInterrupt:
                    # Only notify main thread if we're in a worker thread
                    if threading.current_thread() != threading.main_thread():
                        handle_keyboard_interrupt_properly()
                    raise
                except Exception as e:
                    # If we can't read the file, include the error in the hash
                    hasher.update(f"ERROR:{str(e)}".encode("utf-8"))

        return FingerprintResult(hash=hasher.hexdigest())
    except KeyboardInterrupt:
        # Only notify main thread if we're in a worker thread
        if threading.current_thread() != threading.main_thread():
            handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        return FingerprintResult(hash="", status=f"error: {str(e)}")


def calculate_fingerprint(root_dir: Optional[Path] = None) -> FingerprintResult:
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


def calculate_cpp_test_fingerprint(
    args: Optional[TestArgs] = None,
) -> FingerprintResult:
    """
    Calculate fingerprint for C++ unit tests, including both src/ and tests/ directories.

    Args:
        args: Test arguments (optional, used to include build configuration in fingerprint)

    Returns:
        The fingerprint result covering files that affect C++ unit tests
    """
    start_time = time.time()
    cwd = Path.cwd()

    # Combine fingerprints from both src/ and tests/ directories
    hasher = hashlib.sha256()

    # Process src/ directory (C++ source files)
    src_dir = cwd / "src"
    if src_dir.exists():
        src_result = fingerprint_code_base(src_dir, "**/*.h,**/*.cpp,**/*.hpp")
        hasher.update(f"src:{src_result.hash}".encode("utf-8"))

    # Process tests/ directory (test files)
    tests_dir = cwd / "tests"
    if tests_dir.exists():
        tests_result = fingerprint_code_base(tests_dir, "**/*.h,**/*.cpp,**/*.hpp")
        hasher.update(f"tests:{tests_result.hash}".encode("utf-8"))

    # Include meson.build files that affect build configuration
    meson_build_files = [
        cwd / "meson.build",  # Root build configuration
        cwd / "tests" / "meson.build",  # Test discovery and configuration
    ]
    for meson_file in meson_build_files:
        if meson_file.exists():
            with open(meson_file, "rb") as f:
                meson_content = f.read()
                hasher.update(
                    f"meson:{meson_file.name}:{hashlib.sha256(meson_content).hexdigest()}".encode(
                        "utf-8"
                    )
                )

    # Include build configuration flags that affect compilation
    if args is not None:
        # Include build_mode in fingerprint (quick/debug/release affects compiler flags)
        # This ensures separate fingerprints for different build configurations
        build_mode = (
            args.build_mode if args.build_mode else ("debug" if args.debug else "quick")
        )
        hasher.update(f"build_mode:{build_mode}".encode("utf-8"))

    elapsed_time = time.time() - start_time

    return FingerprintResult(
        hash=hasher.hexdigest(), elapsed_seconds=f"{elapsed_time:.2f}", status="success"
    )


def calculate_examples_fingerprint(
    args: Optional[TestArgs] = None,
) -> FingerprintResult:
    """
    Calculate fingerprint for example tests, including examples/ directory.

    Args:
        args: Test arguments (optional, used to include build configuration in fingerprint)

    Returns:
        The fingerprint result covering files that affect example compilation tests
    """
    start_time = time.time()
    cwd = Path.cwd()

    # Combine fingerprints from relevant directories
    hasher = hashlib.sha256()

    # Process src/ directory (affects example compilation)
    src_dir = cwd / "src"
    if src_dir.exists():
        src_result = fingerprint_code_base(src_dir, "**/*.h,**/*.cpp,**/*.hpp")
        hasher.update(f"src:{src_result.hash}".encode("utf-8"))

    # Process examples/ directory
    examples_dir = cwd / "examples"
    if examples_dir.exists():
        examples_result = fingerprint_code_base(
            examples_dir, "**/*.ino,**/*.h,**/*.cpp,**/*.hpp"
        )
        hasher.update(f"examples:{examples_result.hash}".encode("utf-8"))

    # Include meson.build files that affect build configuration
    meson_build_files = [
        cwd / "meson.build",  # Root build configuration
        cwd / "examples" / "meson.build",  # Example registration and configuration
    ]
    for meson_file in meson_build_files:
        if meson_file.exists():
            with open(meson_file, "rb") as f:
                meson_content = f.read()
                hasher.update(
                    f"meson:{meson_file.name}:{hashlib.sha256(meson_content).hexdigest()}".encode(
                        "utf-8"
                    )
                )

    # Include test_example_compilation.py and related scripts
    example_test_files = [
        cwd / "ci" / "compiler" / "test_example_compilation.py",
        cwd / "ci" / "compiler" / "clang_compiler.py",
        cwd / "ci" / "compiler" / "native_fingerprint.py",
    ]
    for script_file in example_test_files:
        if script_file.exists():
            with open(script_file, "rb") as f:
                script_content = f.read()
                hasher.update(
                    f"script:{script_file.name}:{hashlib.sha256(script_content).hexdigest()}".encode(
                        "utf-8"
                    )
                )

    # Include build configuration flags that affect compilation
    if args is not None:
        # Include build_mode in fingerprint (quick/debug/release affects compiler flags)
        # This ensures separate fingerprints for different build configurations
        build_mode = (
            args.build_mode if args.build_mode else ("debug" if args.debug else "quick")
        )
        hasher.update(f"build_mode:{build_mode}".encode("utf-8"))

    elapsed_time = time.time() - start_time

    return FingerprintResult(
        hash=hasher.hexdigest(), elapsed_seconds=f"{elapsed_time:.2f}", status="success"
    )


def calculate_python_test_fingerprint() -> FingerprintResult:
    """
    Calculate fingerprint for Python tests, including ci/tests/ directory.

    Returns:
        The fingerprint result covering files that affect Python tests
    """
    start_time = time.time()
    cwd = Path.cwd()

    # Combine fingerprints from relevant directories
    hasher = hashlib.sha256()

    # Process ci/tests/ directory (Python test files)
    ci_tests_dir = cwd / "ci" / "tests"
    if ci_tests_dir.exists():
        ci_tests_result = fingerprint_code_base(ci_tests_dir, "**/*.py")
        hasher.update(f"ci_tests:{ci_tests_result.hash}".encode("utf-8"))

    # Process ci/ directory (Python modules that affect tests)
    ci_dir = cwd / "ci"
    if ci_dir.exists():
        # Include important Python modules
        ci_modules_result = fingerprint_code_base(ci_dir, "**/*.py")
        hasher.update(f"ci_modules:{ci_modules_result.hash}".encode("utf-8"))

    # Include relevant configuration files
    config_files = [
        cwd / "pyproject.toml",
        cwd / "uv.lock",
        cwd / ".python-version",
    ]
    for config_file in config_files:
        if config_file.exists():
            with open(config_file, "rb") as f:
                config_content = f.read()
                hasher.update(
                    f"config:{config_file.name}:{hashlib.sha256(config_content).hexdigest()}".encode(
                        "utf-8"
                    )
                )

    elapsed_time = time.time() - start_time

    return FingerprintResult(
        hash=hasher.hexdigest(), elapsed_seconds=f"{elapsed_time:.2f}", status="success"
    )
