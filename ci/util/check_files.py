# pyright: reportUnknownMemberType=false
import os
from abc import ABC, abstractmethod
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from typing import Dict, List, Optional

from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"

NUM_WORKERS = 1 if os.environ.get("NO_PARALLEL") else (os.cpu_count() or 1) * 4

EXCLUDED_FILES = [
    "stub_main.cpp",
]


@dataclass
class FileContent:
    """Container for file content and metadata."""

    path: str
    content: str
    lines: list[str]

    def __post_init__(self):
        if not self.lines:
            self.lines = self.content.splitlines()


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

    def __init__(self, max_workers: Optional[int] = None):
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
        except Exception as e:
            return [f"Error processing file {file_path}: {str(e)}"]


def collect_files_to_check(
    test_directories: list[str], extensions: Optional[list[str]] = None
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

    violations = checker.violations

    if violations:
        total_violations = sum(len(v) for v in violations.values())
        file_count = len(violations)

        print(f"❌ {description} ({file_count} files, {total_violations} violations):")
        print()

        # Sort files by relative path
        for file_path in sorted(violations.keys()):
            rel_path = file_path.replace(str(PROJECT_ROOT), "").lstrip("\\/")
            violation_data = violations[file_path]

            print(f"{rel_path}:")

            # Handle different violation formats
            if isinstance(violation_data, list):
                for item in violation_data:
                    if isinstance(item, tuple) and len(item) >= 2:
                        line_num, content = item[0], item[1]
                        print(f"  Line {line_num}: {content}")
                    else:
                        print(f"  {item}")
            elif isinstance(violation_data, str):
                print(f"  {violation_data}")
            else:
                print(f"  {violation_data}")

            print()

        sys.exit(1)
    else:
        print(f"✅ {description}: No violations found.")
        sys.exit(0)
