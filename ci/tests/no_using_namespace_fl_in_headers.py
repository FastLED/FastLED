#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false, reportMissingParameterType=false
import os
import unittest
from concurrent.futures import ThreadPoolExecutor
from typing import List

from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"
PLATFORMS_DIR = os.path.join(SRC_ROOT, "platforms")

NUM_WORKERS = 1 if os.environ.get("NO_PARALLEL") else (os.cpu_count() or 1) * 4


class NoUsingNamespaceFlInHeaderTester(unittest.TestCase):
    def check_file(self, file_path: str) -> List[str]:
        if "FastLED.h" in file_path:
            return []
        failings: List[str] = []
        with open(file_path, "r", encoding="utf-8") as f:
            for line_number, line in enumerate(f, 1):
                if line.startswith("//"):
                    continue
                if "using namespace fl;" in line:
                    failings.append(f"{file_path}:{line_number}: {line.strip()}")
        return failings

    def test_no_using_namespace(self) -> None:
        """Searches through the program files to check for banned headers, excluding src/platforms."""
        files_to_check: List[str] = []
        for root, _, files in os.walk(SRC_ROOT):
            for file in files:
                if file.endswith(
                    (".h", ".hpp")
                ):  # Add or remove file extensions as needed
                    file_path = os.path.join(root, file)
                    files_to_check.append(file_path)

        all_failings: List[str] = []
        with ThreadPoolExecutor(max_workers=NUM_WORKERS) as executor:
            futures = [
                executor.submit(self.check_file, file_path)
                for file_path in files_to_check
            ]
            for future in futures:
                all_failings.extend(future.result())

        if all_failings:
            msg = (
                f'Found {len(all_failings)} header file(s) "using namespace fl": \n'
                + "\n".join(all_failings)
            )
            for failing in all_failings:
                print(failing)
            self.fail(msg)
        else:
            print("No using namespace fl; found in headers.")


if __name__ == "__main__":
    unittest.main()
