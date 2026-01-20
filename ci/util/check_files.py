# pyright: reportUnknownMemberType=false
import os
from abc import ABC, abstractmethod
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass

from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"

NUM_WORKERS = 1 if os.environ.get("NO_PARALLEL") else (os.cpu_count() or 1) * 4

EXCLUDED_FILES = [
    "stub_main.cpp",
    "doctest_main.cpp",  # Binding to system headers, needs exemption
    "fltest.h",  # Test framework needs <exception> when exceptions enabled
    "doctest.h",  # Third-party test framework - external dependency
    # Test infrastructure for crash handling - uses platform-specific features
    "crash_handler_execinfo.h",
    "crash_handler_libunwind.h",
    "crash_handler_noop.h",
    "crash_handler_win.h",
]


def is_excluded_file(file_path: str) -> bool:
    """Check if a file should be excluded from linting.

    Normalizes path separators to handle both Windows and Unix paths.
    """
    # Normalize to forward slashes for consistent matching
    normalized_path = file_path.replace("\\", "/")
    return any(normalized_path.endswith(excluded) for excluded in EXCLUDED_FILES)


@dataclass
class FileContent:
    """Container for file content and metadata."""

    path: str
    content: str
    lines: list[str]

    def __post_init__(self):
        if not self.lines:
            self.lines = self.content.splitlines()


@dataclass
class Violation:
    """Container for a single violation at a specific line."""

    line_number: int
    content: str


@dataclass
class FileViolations:
    """Container for violations in a single file."""

    file_path: str
    violations: list[Violation]

    def __init__(self, file_path: str):
        """Initialize with empty violations list."""
        self.file_path = file_path
        self.violations = []

    def add_violation(self, line_number: int, content: str) -> None:
        """Add a violation to this file."""
        self.violations.append(Violation(line_number, content))

    def violation_count(self) -> int:
        """Get the number of violations in this file."""
        return len(self.violations)

    def has_violations(self) -> bool:
        """Check if there are any violations."""
        return len(self.violations) > 0


@dataclass
class CheckerResults:
    """Container for all violations found by a checker."""

    violations: dict[str, FileViolations]

    def __init__(self):
        """Initialize with empty violations dict."""
        self.violations = {}

    def add_violation(self, file_path: str, line_number: int, content: str) -> None:
        """Add a violation for a file."""
        if file_path not in self.violations:
            self.violations[file_path] = FileViolations(file_path)
        self.violations[file_path].add_violation(line_number, content)

    def total_violations(self) -> int:
        """Get total number of violations across all files."""
        return sum(fv.violation_count() for fv in self.violations.values())

    def file_count(self) -> int:
        """Get number of files with violations."""
        return len(self.violations)

    def has_violations(self) -> bool:
        """Check if any violations were found."""
        return len(self.violations) > 0


class FileContentChecker(ABC):
    """Abstract base class for checking file content."""

    @abstractmethod
    def should_process_file(self, file_path: str) -> bool:
        """Predicate to determine if a file should be processed.

        Args:
            file_path: Path to the file to check

        Returns:
            True if the file should be processed, False otherwise
        """
        pass

    @abstractmethod
    def check_file_content(self, file_content: FileContent) -> list[str]:
        """Check the file content and return any issues found.

        Args:
            file_content: FileContent object containing path, content, and lines

        Returns:
            List of error messages, empty if no issues found
        """
        pass


class MultiCheckerFileProcessor:
    """Processor that can run multiple checkers on files."""

    def __init__(self):
        pass

    def process_files_with_checkers(
        self, file_paths: list[str], checkers: list[FileContentChecker]
    ) -> dict[str, list[str]]:
        """Process files with multiple checkers.

        Args:
            file_paths: List of file paths to process
            checkers: List of checker instances to run on the files

        Returns:
            Dictionary mapping checker class name to list of issues found
        """
        # Initialize results dictionary for each checker
        results: dict[str, list[str]] = {}
        for checker in checkers:
            checker_name = checker.__class__.__name__
            results[checker_name] = []

        # Process each file
        for file_path in file_paths:
            # Check if any checker wants to process this file
            interested_checkers = [
                checker
                for checker in checkers
                if checker.should_process_file(file_path)
            ]

            # If any checker is interested, read the file once
            if interested_checkers:
                try:
                    with open(file_path, "r", encoding="utf-8") as f:
                        content = f.read()

                    # Create FileContent object with lines split
                    file_content = FileContent(
                        path=file_path, content=content, lines=content.splitlines()
                    )

                    # Pass the file content to all interested checkers
                    for checker in interested_checkers:
                        checker_name = checker.__class__.__name__
                        issues = checker.check_file_content(file_content)
                        results[checker_name].extend(issues)

                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    raise
                except Exception as e:
                    # Add error to all interested checkers
                    error_msg = f"Error reading file {file_path}: {str(e)}"
                    for checker in interested_checkers:
                        checker_name = checker.__class__.__name__
                        results[checker_name].append(error_msg)

        return results


# Legacy compatibility classes
class FileProcessorCallback(FileContentChecker):
    """Legacy compatibility wrapper - delegates to FileContentChecker methods."""

    def check_file_content_legacy(self, file_path: str, content: str) -> list[str]:
        """Legacy method signature for backward compatibility."""
        file_content = FileContent(path=file_path, content=content, lines=[])
        return self.check_file_content(file_content)


class GenericFileSearcher:
    """Generic file searcher that processes files using a callback pattern."""

    def __init__(self, max_workers: int | None = None):
        self.max_workers = max_workers or NUM_WORKERS

    def search_directory(
        self, start_dir: str, callback: FileProcessorCallback
    ) -> list[str]:
        """Search a directory and process files using the provided callback.

        Args:
            start_dir: Directory to start searching from
            callback: Callback class to handle file processing

        Returns:
            List of all issues found across all files
        """
        files_to_check: list[str] = []

        # Collect all files that should be processed
        for root, _, files in os.walk(start_dir):
            for file in files:
                file_path = os.path.join(root, file)
                if callback.should_process_file(file_path):
                    files_to_check.append(file_path)

        # Process files in parallel
        all_issues: list[str] = []
        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            futures = [
                executor.submit(self._process_single_file, file_path, callback)
                for file_path in files_to_check
            ]
            for future in futures:
                all_issues.extend(future.result())

        return all_issues

    def _process_single_file(
        self, file_path: str, callback: FileProcessorCallback
    ) -> list[str]:
        """Process a single file using the callback.

        Args:
            file_path: Path to the file to process
            callback: Callback to use for processing

        Returns:
            List of issues found in this file
        """
        try:
            with open(file_path, "r", encoding="utf-8") as f:
                content = f.read()
            file_content = FileContent(path=file_path, content=content, lines=[])
            return callback.check_file_content(file_content)
            handle_keyboard_interrupt_properly()
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            return [f"Error processing file {file_path}: {str(e)}"]


def collect_files_to_check(
    test_directories: list[str], extensions: list[str] | None = None
) -> list[str]:
    """Collect all files to check from the given directories."""
    if extensions is None:
        extensions = [".cpp", ".h", ".hpp"]

    files_to_check: list[str] = []

    # Search each directory
    for directory in test_directories:
        if os.path.exists(directory):
            for root, _, files in os.walk(directory):
                for file in files:
                    if any(file.endswith(ext) for ext in extensions):
                        file_path = os.path.join(root, file)
                        files_to_check.append(file_path)

    # Only add main src directory files if SRC_ROOT itself is in test_directories
    # This prevents unintentionally including src/*.{cpp,h} when testing subdirectories
    src_root_str = str(SRC_ROOT)
    if src_root_str in test_directories or any(
        d == src_root_str for d in test_directories
    ):
        for file in os.listdir(SRC_ROOT):
            file_path = os.path.join(SRC_ROOT, file)
            if os.path.isfile(file_path) and any(
                file_path.endswith(ext) for ext in extensions
            ):
                files_to_check.append(file_path)

    return files_to_check


def run_checker_standalone(
    checker: FileContentChecker,
    directories: list[str],
    description: str,
    extensions: list[str] | None = None,
) -> None:
    """Run a single checker standalone with standard output formatting.

    Args:
        checker: Checker instance to run
        directories: List of directories to scan
        description: Human-readable description of what's being checked
        extensions: File extensions to check (default: [".cpp", ".h", ".hpp"])
    """
    import sys

    # Collect files
    files_to_check = collect_files_to_check(directories, extensions)

    # Run checker
    processor = MultiCheckerFileProcessor()
    processor.process_files_with_checkers(files_to_check, [checker])

    # Get violations (checkers store them in .violations attribute)
    if not hasattr(checker, "violations"):
        print(f"✅ {description}: No violations found.")
        sys.exit(0)

    violations = getattr(checker, "violations")

    # Support both legacy dict format and new CheckerResults format
    if isinstance(violations, CheckerResults):
        results = violations
    else:
        # Legacy format - convert to CheckerResults
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

    if results.has_violations():
        total_violations = results.total_violations()
        file_count = results.file_count()

        print(f"❌ {description} ({file_count} files, {total_violations} violations):")
        print()

        # Sort files by relative path
        for file_path in sorted(results.violations.keys()):
            rel_path = file_path.replace(str(PROJECT_ROOT), "").lstrip("\\/")
            file_violations = results.violations[file_path]

            print(f"{rel_path}:")
            for violation in file_violations.violations:
                print(f"  Line {violation.line_number}: {violation.content}")
            print()

        sys.exit(1)
    else:
        print(f"✅ {description}: No violations found.")
        sys.exit(0)
