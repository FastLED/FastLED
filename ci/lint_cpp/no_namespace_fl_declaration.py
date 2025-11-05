#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false, reportMissingParameterType=false
"""Linter to check for 'namespace fl {' declarations in src/ root files.

This prevents namespace fl declarations directly in src/*.h and src/*.cpp files,
but allows them in subdirectories like src/chipsets/, src/protocols/, etc.
"""

import os
import re
import unittest
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from typing import List

from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"

NUM_WORKERS = 1 if os.environ.get("NO_PARALLEL") else (os.cpu_count() or 1) * 4


class NoNamespaceFlDeclarationTester(unittest.TestCase):
    def check_file(self, file_path: str) -> list[str]:
        """Check if a file has namespace fl { declarations."""
        failings: list[str] = []
        try:
            with open(file_path, "r", encoding="utf-8") as f:
                for line_number, line in enumerate(f, 1):
                    # Skip comment lines
                    if line.strip().startswith("//"):
                        continue
                    # Match 'namespace fl {' pattern
                    if re.search(r"\bnamespace\s+fl\s*\{", line):
                        failings.append(f"{file_path}:{line_number}: {line.strip()}")
        except (UnicodeDecodeError, IOError):
            # Skip files that can't be read
            pass
        return failings

    def test_no_namespace_fl_in_src_root(self) -> None:
        """Check that 'namespace fl {' is not in src/*.h and src/*.cpp files.

        This only checks files directly in src/, not in subdirectories like
        src/chipsets/, src/protocols/, etc.
        """
        files_to_check: list[str] = []

        # Only check files directly in src/ directory, not subdirectories
        try:
            src_path = Path(SRC_ROOT)
            for file_path in src_path.glob("*"):
                # Only process .h and .cpp files directly in src/
                if file_path.is_file() and file_path.suffix in [".h", ".cpp"]:
                    files_to_check.append(str(file_path))
        except (OSError, IOError):
            pass

        all_failings: list[str] = []
        with ThreadPoolExecutor(max_workers=NUM_WORKERS) as executor:
            futures = [
                executor.submit(self.check_file, file_path)
                for file_path in files_to_check
            ]
            for future in futures:
                all_failings.extend(future.result())

        if all_failings:
            msg = (
                f"Found {len(all_failings)} file(s) with 'namespace fl {{' "
                f"in src/ root directory (not allowed):\n" + "\n".join(all_failings)
            )
            for failing in all_failings:
                print(failing)
            self.fail(msg)
        else:
            print("✅ No 'namespace fl {' declarations found in src/ root files.")


if __name__ == "__main__":
    unittest.main()
