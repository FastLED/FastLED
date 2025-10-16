# pyright: reportUnknownMemberType=false
import os
import unittest
from typing import Callable, List

from ci.util.check_files import (
    EXCLUDED_FILES,
    FileContent,
    FileContentChecker,
    MultiCheckerFileProcessor,
    collect_files_to_check,
)
from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"

ENABLE_PARANOID_GNU_HEADER_INSPECTION = False

if ENABLE_PARANOID_GNU_HEADER_INSPECTION:
    BANNED_HEADERS_ESP = ["esp32-hal.h"]
else:
    BANNED_HEADERS_ESP = []

BANNED_HEADERS_COMMON = [
    "pthread.h",
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
    "stdint.h",
    "stddef.h",
    "cstddef",  # this certainally fails
    "type_traits",  # this certainally fails
    "new",  # Ban <new> except for placement new in inplacenew.h
]

BANNED_HEADERS_CORE = BANNED_HEADERS_COMMON + BANNED_HEADERS_ESP + ["Arduino.h"]

# Banned headers for platforms directory - specifically checking for Arduino.h
BANNED_HEADERS_PLATFORMS = ["Arduino.h"]


class BannedHeadersChecker(FileContentChecker):
    """Checker class for banned headers."""

    def __init__(self, banned_headers_list: List[str], strict_mode: bool = False):
        """Initialize with the list of banned headers to check for.

        Args:
            banned_headers_list: List of headers to ban
            strict_mode: If True, do not allow "// ok include" bypass (for fl/ directory)
        """
        self.banned_headers_list = banned_headers_list
        self.strict_mode = strict_mode

    def should_process_file(self, file_path: str) -> bool:
        """Check if file should be processed for banned headers."""
        # Check file extension
        if not file_path.endswith((".cpp", ".h", ".hpp", ".ino")):
            return False

        # Check if file is in excluded list
        if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
            return False

        return True

    def _is_allowed_exception(self, file_path: str, header: str) -> bool:
        """Check if this is an allowed exception to the banned header rule."""
        # Allow <new> in inplacenew.h for placement new operator
        if header == "new" and "inplacenew.h" in file_path:
            return True

        # For fl/ directory, allow specific platform headers that have no alternatives
        # These are genuinely needed for platform-specific implementations
        if "/fl/" in file_path.replace("\\", "/"):
            # Allow pthread.h in thread-local implementation
            if header == "pthread.h" and "thread_local" in file_path:
                return True

            # Allow mutex in mutex.h multithreading implementation
            if header == "mutex" and "mutex.h" in file_path:
                return True

            # Allow memory in thread_local.h for std::unique_ptr/shared_ptr (used with pthread)
            if header == "memory" and "thread_local.h" in file_path:
                return True

            # Allow platform-specific time headers in time.cpp
            if "time.cpp" in file_path:
                if header in {"stdint.h", "Arduino.h", "chrono"}:
                    return True

        return False

    def check_file_content(self, file_content: FileContent) -> List[str]:
        """Check file content for banned headers."""
        failings: List[str] = []

        if len(self.banned_headers_list) == 0:
            return failings

        # Check each line for banned headers
        for line_number, line in enumerate(file_content.lines, 1):
            if line.strip().startswith("//"):
                continue

            for header in self.banned_headers_list:
                if f"#include <{header}>" in line or f'#include "{header}"' in line:
                    # Check if this is an allowed exception
                    if self._is_allowed_exception(file_content.path, header):
                        continue

                    # In strict mode, do not allow "// ok include" bypass
                    if not self.strict_mode and "// ok include" in line:
                        continue

                    if self.strict_mode:
                        failings.append(
                            f"Found banned header '{header}' in {file_content.path}:{line_number} (fl/ directory: strict mode, no bypass allowed)"
                        )
                    else:
                        failings.append(
                            f"Found banned header '{header}' in {file_content.path}:{line_number}"
                        )

        return failings


def _test_no_banned_headers(
    test_directories: List[str],
    banned_headers_list: List[str],
    on_fail: Callable[[str], None],
    strict_mode: bool = False,
) -> None:
    """Searches through the program files to check for banned headers.

    Args:
        test_directories: Directories to check
        banned_headers_list: List of banned headers
        on_fail: Callback to call on failure
        strict_mode: If True, do not allow "// ok include" bypass (for fl/ directory)
    """
    # Collect files to check
    files_to_check = collect_files_to_check(test_directories)

    # Create processor and checker
    processor = MultiCheckerFileProcessor()
    checker = BannedHeadersChecker(banned_headers_list, strict_mode=strict_mode)

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
    def test_no_banned_headers_fl(self) -> None:
        """Searches through fl/ directory with STRICT enforcement - no bypass allowed."""

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n"
                "fl/ directory uses STRICT mode: banned headers are not allowed, even with '// ok include' comment."
            )

        # Test fl/ directory with strict mode
        test_directories = [
            os.path.join(SRC_ROOT, "fl"),
        ]
        _test_no_banned_headers(
            test_directories=test_directories,
            banned_headers_list=BANNED_HEADERS_CORE,
            on_fail=on_fail,
            strict_mode=True,
        )

    def test_no_banned_headers_src(self) -> None:
        """Searches through fx/, sensors/, and platforms/shared/ directories for banned headers."""

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n"
                "You can add '// ok include' at the end of the line to silence this error for specific inclusions."
            )

        # Test other source directories (not fl/)
        test_directories = [
            os.path.join(SRC_ROOT, "fx"),
            os.path.join(SRC_ROOT, "sensors"),
            os.path.join(SRC_ROOT, "platforms", "shared"),
        ]
        _test_no_banned_headers(
            test_directories=test_directories,
            banned_headers_list=BANNED_HEADERS_CORE,
            on_fail=on_fail,
            strict_mode=False,
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

    def test_no_banned_headers_platforms(self) -> None:
        """Searches through the platforms directory to check for Arduino.h usage."""

        def on_fail(msg: str) -> None:
            self.fail(
                msg + "\n"
                "You can add '// ok include' at the end of the line to silence this error for specific inclusions."
            )

        # Test the platforms directory specifically for Arduino.h
        test_directories = [
            os.path.join(SRC_ROOT, "platforms"),
        ]
        _test_no_banned_headers(
            test_directories=test_directories,
            banned_headers_list=BANNED_HEADERS_PLATFORMS,
            on_fail=on_fail,
        )


if __name__ == "__main__":
    unittest.main()
