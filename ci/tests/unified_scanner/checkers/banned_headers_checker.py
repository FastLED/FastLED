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

    def check_file(self, file_path: Path, content: str) -> List[CheckResult]:
        results: List[CheckResult] = []
        lines = content.split("\n")

        # Get appropriate banned list for this file's location
        banned_headers = self._get_banned_headers_for_path(file_path)

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
                    # Allow if there's a // ok include comment
                    if "// ok include" in line or "// OK include" in line:
                        continue

                    results.append(
                        CheckResult(
                            checker_name=self.name(),
                            file_path=str(file_path),
                            line_number=line_num,
                            severity="ERROR",
                            message=f"Banned header: {banned}",
                            suggestion=f"Remove {banned} or add '// ok include' comment if required",
                        )
                    )

        return results
