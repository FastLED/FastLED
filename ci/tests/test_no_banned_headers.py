import os
import unittest
from concurrent.futures import ThreadPoolExecutor

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


BANNED_HEADERS_CORE = [
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
    "Arduino.h",
] + BANNED_HEADERS_ESP

EXCLUDED_FILES = [
    "stub_main.cpp",
]


class TestBinToElf(unittest.TestCase):

    def check_file(self, file_path: str) -> list[str]:
        failings: list[str] = []
        banned_headers_list = []
        if file_path.startswith(PLATFORMS_DIR):
            # continue  # Skip the platforms directory
            if file_path.startswith(PLATFORMS_ESP_DIR):
                banned_headers_list = BANNED_HEADERS_ESP
            else:
                return failings
        if len(banned_headers_list) == 0:
            return failings
        with open(file_path, "r", encoding="utf-8") as f:

            for line_number, line in enumerate(f, 1):
                if line.startswith("//"):
                    continue
                for header in banned_headers_list:
                    if (
                        f"#include <{header}>" in line or f'#include "{header}"' in line
                    ) and "// ok include" not in line:
                        failings.append(
                            f"Found banned header '{header}' in {file_path}:{line_number}"
                        )
        return failings

    def test_no_banned_headers(self) -> None:
        """Searches through the program files to check for banned headers, excluding src/platforms."""
        files_to_check = []
        for root, _, files in os.walk(SRC_ROOT):
            for file in files:
                if file.endswith(
                    (".cpp", ".h", ".hpp")
                ):  # Add or remove file extensions as needed
                    file_path = os.path.join(root, file)
                    if not any(
                        file_path.endswith(excluded) for excluded in EXCLUDED_FILES
                    ):
                        files_to_check.append(file_path)

        all_failings = []
        with ThreadPoolExecutor(max_workers=NUM_WORKERS) as executor:
            futures = [
                executor.submit(self.check_file, file_path)
                for file_path in files_to_check
            ]
            for future in futures:
                all_failings.extend(future.result())

        if all_failings:
            msg = f"Found {len(all_failings)} banned header(s): \n" + "\n".join(
                all_failings
            )
            for failing in all_failings:
                print(failing)
            self.fail(
                msg + "\n"
                "You can add '// ok include' at the end of the line to silence this error for specific inclusions."
            )
        else:
            print("No banned headers found.")


if __name__ == "__main__":
    unittest.main()
