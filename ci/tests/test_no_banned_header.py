import os
import unittest

from ci.paths import PROJECT_ROOT

SRC_ROOT = PROJECT_ROOT / "src"
PLATFORMS_DIR = os.path.join(SRC_ROOT, "platforms")

BANNED_HEADERS = [
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
]

EXCLUDED_FILES = [
    "stub_main.cpp",
]


class TestBinToElf(unittest.TestCase):

    def test_no_banned_headers(self) -> None:
        """Searches through the program files to check for banned headers, excluding src/platforms."""
        failings = []
        for root, _, files in os.walk(SRC_ROOT):
            if root.startswith(PLATFORMS_DIR):
                continue  # Skip the platforms directory
            for file in files:
                if file.endswith(
                    (".cpp", ".h", ".hpp")
                ):  # Add or remove file extensions as needed
                    file_path = os.path.join(root, file)
                    if any(file_path.endswith(excluded) for excluded in EXCLUDED_FILES):
                        continue  # Skip excluded files
                    with open(file_path, "r", encoding="utf-8") as f:
                        for line_number, line in enumerate(f, 1):
                            for header in BANNED_HEADERS:
                                if f"#include <{header}>" in line:
                                    failings.append(
                                        f"Found banned header '{header}' in {file_path}:{line_number}"
                                    )

        if failings:
            for failing in failings:
                print(failing)
            self.fail(f"Found {len(failings)} banned header(s). See above for details.")
        else:
            print("No banned headers found.")


if __name__ == "__main__":
    unittest.main()
