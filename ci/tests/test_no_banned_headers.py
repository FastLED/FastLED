import os
import unittest
from abc import ABC, abstractmethod
from concurrent.futures import ThreadPoolExecutor
from typing import Callable, List, Optional

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


class FileProcessorCallback(ABC):
    """Abstract base class for file processing callbacks."""

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
    def check_file_content(self, file_path: str, content: str) -> List[str]:
        """Check the file content and return any issues found.

        Args:
            file_path: Path to the file being checked
            content: Content of the file as a string

        Returns:
            List of error messages, empty if no issues found
        """
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
            return callback.check_file_content(file_path, content)
        except Exception as e:
            return [f"Error processing file {file_path}: {str(e)}"]


class BannedHeadersCallback(FileProcessorCallback):
    """Callback class for checking banned headers."""

    def __init__(self, banned_headers_list: List[str]):
        """Initialize with the list of banned headers to check for."""
        self.banned_headers_list = banned_headers_list

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for banned headers."""
        # Check file extension
        if not file_path.endswith((".cpp", ".h", ".hpp")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        return True

    def check_file_content(self, file_path: str, content: str) -> List[str]:
        """Check file content for banned headers."""
        failings = []

        if len(self.banned_headers_list) == 0:
            return failings

        # Check each line for banned headers
        for line_number, line in enumerate(content.splitlines(), 1):
            if line.strip().startswith("//"):
                continue

            for header in self.banned_headers_list:
                if (
                    f"#include <{header}>" in line or f'#include "{header}"' in line
                ) and "// ok include" not in line:
                    failings.append(
                        f"Found banned header '{header}' in {file_path}:{line_number}"
                    )

        return failings


def _test_no_banned_headers(
    test_directories: list[str],
    banned_headers_list: List[str],
    on_fail: Callable[[str], None],
) -> None:
    """Searches through the program files to check for banned headers."""
    searcher = GenericFileSearcher()
    callback = BannedHeadersCallback(banned_headers_list)

    all_failings = []

    # Search each directory
    for directory in test_directories:
        if os.path.exists(directory):
            directory_failings = searcher.search_directory(directory, callback)
            all_failings.extend(directory_failings)

    # Also check the main src directory files (not subdirectories)
    main_src_files = []
    for file in os.listdir(SRC_ROOT):
        file_path = os.path.join(SRC_ROOT, file)
        if os.path.isfile(file_path) and callback.should_process_file(file_path):
            main_src_files.append(file_path)

    # Process main src files
    for file_path in main_src_files:
        try:
            with open(file_path, "r", encoding="utf-8") as f:
                content = f.read()
            file_failings = callback.check_file_content(file_path, content)
            all_failings.extend(file_failings)
        except Exception as e:
            all_failings.append(f"Error processing file {file_path}: {str(e)}")

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
