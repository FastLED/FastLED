import os
import unittest
from abc import ABC, abstractmethod
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
from typing import Callable, Dict, List, Optional

from ci.paths import PROJECT_ROOT

SRC_ROOT = PROJECT_ROOT / "src"
PLATFORMS_DIR = os.path.join(SRC_ROOT, "platforms")
PLATFORMS_ESP_DIR = os.path.join(PLATFORMS_DIR, "esp")

NUM_WORKERS = (os.cpu_count() or 1) * 4

ENABLE_PARANOID_GNU_HEADER_INSPECTION = False

if ENABLE_PARANOID_GNU_HEADER_INSPECTION:
    BANNED_HEADERS_ESP = ["esp32-hal.h"]
else:
    BANNED_HEADERS_ESP = []

BANNED_HEADERS_COMMON = [
    "assert.h",
    "iostream",
    "stdio.h",
    "cstdio",
    "cstdlib",
    "vector",
    "list",
    "map",
    "set",
    "queue",
    "deque",
    "algorithm",
    "memory",
    "thread",
    "mutex",
    "chrono",
    "fstream",
    "sstream",
    "iomanip",
    "exception",
    "stdexcept",
    "typeinfo",
    "ctime",
    "cmath",
    "complex",
    "valarray",
    "cfloat",
    "cassert",
    "cerrno",
    "cctype",
    "cwctype",
    "cstring",
    "cwchar",
    "cuchar",
    "cstdint",
    "cstddef",  # this certainally fails
    "type_traits",  # this certainally fails
]

BANNED_HEADERS_CORE = BANNED_HEADERS_COMMON + BANNED_HEADERS_ESP + ["Arduino.h"]

EXCLUDED_FILES = [
    "stub_main.cpp",
]


@dataclass
class FileContent:
    """Container for file content and metadata."""

    path: str
    content: str
    lines: List[str]

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
    def check_file_content(self, file_content: FileContent) -> List[str]:
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
        self, file_paths: List[str], checkers: List[FileContentChecker]
    ) -> Dict[str, List[str]]:
        """Process files with multiple checkers.

        Args:
            file_paths: List of file paths to process
            checkers: List of checker instances to run on the files

        Returns:
            Dictionary mapping checker class name to list of issues found
        """
        # Initialize results dictionary for each checker
        results = {}
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


class BannedHeadersChecker(FileContentChecker):
    """Checker class for banned headers."""

    def __init__(self, banned_headers_list: List[str]):
        """Initialize with the list of banned headers to check for."""
        self.banned_headers_list = banned_headers_list

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for banned headers."""
        # Check file extension
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        return True

    def check_file_content(self, file_content: FileContent) -> List[str]:
        """Check file content for banned headers."""
        failings = []

        if len(self.banned_headers_list) == 0:
            return failings

        # Check each line for banned headers
        for line_number, line in enumerate(file_content.lines, 1):
            if line.strip().startswith("//"):
                continue

            for header in self.banned_headers_list:
                if (
                    f"#include <{header}>" in line or f'#include "{header}"' in line
                ) and "// ok include" not in line:
                    failings.append(
                        f"Found banned header '{header}' in {file_content.path}:{line_number}"
                    )

        return failings


# Legacy compatibility classes
class FileProcessorCallback(FileContentChecker):
    """Legacy compatibility wrapper - delegates to FileContentChecker methods."""

    def check_file_content_legacy(self, file_path: str, content: str) -> List[str]:
        """Legacy method signature for backward compatibility."""
        file_content = FileContent(path=file_path, content=content, lines=[])
        return self.check_file_content(file_content)


class BannedHeadersCallback(BannedHeadersChecker):
    """Legacy compatibility class."""

    pass


class GenericFileSearcher:
    """Generic file searcher that processes files using a callback pattern."""

    def __init__(self, max_workers: Optional[int] = None):
        self.max_workers = max_workers or NUM_WORKERS

    def search_directory(
        self, start_dir: str, callback: FileProcessorCallback
    ) -> List[str]:
        """Search a directory and process files using the provided callback.

        Args:
            start_dir: Directory to start searching from
            callback: Callback class to handle file processing

        Returns:
            List of all issues found across all files
        """
        files_to_check = []

        # Collect all files that should be processed
        for root, _, files in os.walk(start_dir):
            for file in files:
                file_path = os.path.join(root, file)
                if callback.should_process_file(file_path):
                    files_to_check.append(file_path)

        # Process files in parallel
        all_issues = []
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
    ) -> List[str]:
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


def _collect_files_to_check(
    test_directories: List[str], extensions: List[str] | None = None
) -> List[str]:
    """Collect all files to check from the given directories."""
    if extensions is None:
        extensions = [".cpp", ".h", ".hpp"]

    files_to_check = []

    # Search each directory
    for directory in test_directories:
        if os.path.exists(directory):
            for root, _, files in os.walk(directory):
                for file in files:
                    if any(file.endswith(ext) for ext in extensions):
                        file_path = os.path.join(root, file)
                        files_to_check.append(file_path)

    # Also check the main src directory files (not subdirectories)
    for file in os.listdir(SRC_ROOT):
        file_path = os.path.join(SRC_ROOT, file)
        if os.path.isfile(file_path) and any(
            file_path.endswith(ext) for ext in extensions
        ):
            files_to_check.append(file_path)

    return files_to_check


def _test_no_banned_headers(
    test_directories: list[str],
    banned_headers_list: List[str],
    on_fail: Callable[[str], None],
) -> None:
    """Searches through the program files to check for banned headers."""
    # Collect files to check
    files_to_check = _collect_files_to_check(test_directories)

    # Create processor and checker
    processor = MultiCheckerFileProcessor()
    checker = BannedHeadersChecker(banned_headers_list)

    # Process files
    results = processor.process_files_with_checkers(files_to_check, [checker])

    # Get results for banned headers checker
    all_failings = results.get("BannedHeadersChecker", []) or []

    if all_failings:
        msg = f"Found {len(all_failings)} banned header(s): \n" + "\n".join(
            all_failings
        )
        for failing in all_failings:
            print(failing)

        on_fail(msg)
    else:
        print("No banned headers found.")


class TestNoBannedHeaders(unittest.TestCase):

    def test_no_banned_headers_src(self) -> None:
        """Searches through the program files to check for banned headers."""

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n"
                "You can add '// ok include' at the end of the line to silence this error for specific inclusions."
            )

        # Test directories as requested
        test_directories = [
            os.path.join(SRC_ROOT, "fl"),
            os.path.join(SRC_ROOT, "fx"),
            os.path.join(SRC_ROOT, "sensors"),
        ]
        _test_no_banned_headers(
            test_directories=test_directories,
            banned_headers_list=BANNED_HEADERS_CORE,
            on_fail=on_fail,
        )

    def test_no_banned_headers_examples(self) -> None:
        """Searches through the program files to check for banned headers."""

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n"
                "You can add '// ok include' at the end of the line to silence this error for specific inclusions."
            )

        test_directories = ["examples"]

        _test_no_banned_headers(
            test_directories=test_directories,
            banned_headers_list=BANNED_HEADERS_COMMON,
            on_fail=on_fail,
        )


if __name__ == "__main__":
    unittest.main()
