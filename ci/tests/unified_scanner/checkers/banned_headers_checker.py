# pyright: reportUnknownMemberType=false
from pathlib import Path
from typing import List, Set

from ..base_checker import BaseChecker
from ..result import CheckResult


class BannedHeadersChecker(BaseChecker):
    """Checks for banned header includes."""

    # Common banned headers - use FastLED alternatives instead
    BANNED_HEADERS_COMMON: Set[str] = {
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
        "cstddef",
        "type_traits",
        "new",  # Ban <new> except for placement new in inplacenew.h
    }

    # ESP-specific banned headers (only if paranoid mode enabled)
    BANNED_HEADERS_ESP: Set[str] = (
        set()
    )  # Empty for now, could add "esp32-hal.h" if needed

    # Platform-specific: Arduino.h should only be in platforms/, not core
    BANNED_HEADERS_CORE: Set[str] = BANNED_HEADERS_COMMON | {"Arduino.h"}

    # For platforms/ directory: only ban Arduino.h
    BANNED_HEADERS_PLATFORMS: Set[str] = {"Arduino.h"}

    def name(self) -> str:
        return "banned-headers"

    def should_check(self, file_path: Path) -> bool:
        if file_path.suffix not in {".cpp", ".h", ".hpp", ".cc", ".ino"}:
            return False

        # Skip third_party directories (they can use any headers)
        path_str = str(file_path)
        if "third_party" in path_str.replace("\\", "/").split("/"):
            return False

        return True

    def _get_banned_headers_for_path(self, file_path: Path) -> Set[str]:
        """Return the appropriate banned headers list based on file path."""
        path_str = str(file_path).replace("\\", "/")
        path_parts = path_str.split("/")

        # Check if in tests/ directory
        if "tests" in path_parts:
            # Tests can use standard library headers for testing infrastructure
            return set()  # Don't ban anything in tests

        # Check if in examples/ directory
        if "examples" in path_parts:
            # Examples can use standard headers for demonstration purposes
            return set()  # Don't ban anything in examples

        # Check if in platforms/ directory
        if "platforms" in path_parts:
            return self.BANNED_HEADERS_PLATFORMS

        # Default: core source (includes Arduino.h ban)
        return self.BANNED_HEADERS_CORE

    def _is_in_fl_directory(self, file_path: Path) -> bool:
        """Check if file is in the fl/ directory."""
        path_str = str(file_path).replace("\\", "/")
        return "/fl/" in path_str or path_str.endswith("/fl") or "/src/fl/" in path_str

    def _is_allowed_exception(
        self, file_path: Path, banned_header: str, line: str
    ) -> bool:
        """Check if this is an allowed exception to the banned header rule."""
        file_path_str = str(file_path)

        # Allow <new> in inplacenew.h for placement new operator
        if banned_header == "new" and "inplacenew.h" in file_path_str:
            return True

        # For fl/ directory, allow specific platform headers that have no alternatives
        # These are genuinely needed for platform-specific implementations
        if "/fl/" in file_path_str.replace("\\", "/"):
            # Allow pthread.h in thread-local implementation
            if banned_header == "pthread.h" and "thread_local" in file_path_str:
                return True

            # Allow mutex in mutex.h multithreading implementation
            if banned_header == "mutex" and "mutex.h" in file_path_str:
                return True

            # Allow memory in thread_local.h for std::unique_ptr/shared_ptr (used with pthread)
            if banned_header == "memory" and "thread_local.h" in file_path_str:
                return True

            # Allow platform-specific time headers in time.cpp
            if "time.cpp" in file_path_str:
                if banned_header in {"stdint.h", "Arduino.h", "chrono"}:
                    return True

        return False

    def check_file(self, file_path: Path, content: str) -> List[CheckResult]:
        results: List[CheckResult] = []
        lines = content.split("\n")

        # Get appropriate banned list for this file's location
        banned_headers = self._get_banned_headers_for_path(file_path)

        # Check if in fl/ directory for strict enforcement
        is_fl_dir = self._is_in_fl_directory(file_path)

        for line_num, line in enumerate(lines, 1):
            stripped = line.strip()

            # Check for #include
            if not stripped.startswith("#include"):
                continue

            # Check for banned headers
            for banned in banned_headers:
                # Check both <header> and "header" formats
                if (
                    f"<{banned}>" in stripped
                    or f'"{banned}"' in stripped
                    or f"<{banned}" in stripped  # Handles cases like <iostream
                ):
                    # Check if this is an allowed exception (e.g., <new> in inplacenew.h)
                    if self._is_allowed_exception(file_path, banned, line):
                        continue

                    # For fl/ directory: STRICT mode - no "// ok include" bypass allowed
                    # For other directories: allow "// ok include" bypass
                    if not is_fl_dir:
                        if "// ok include" in line or "// OK include" in line:
                            continue

                    # Generate appropriate error message
                    if is_fl_dir:
                        suggestion = f"Remove {banned}. fl/ directory headers must not use banned standard library headers."
                    else:
                        suggestion = f"Remove {banned} or add '// ok include' comment if required"

                    results.append(
                        CheckResult(
                            checker_name=self.name(),
                            file_path=str(file_path),
                            line_number=line_num,
                            severity="ERROR",
                            message=f"Banned header: {banned}",
                            suggestion=suggestion,
                        )
                    )

        return results
