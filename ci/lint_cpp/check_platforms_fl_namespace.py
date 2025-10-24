#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false, reportMissingParameterType=false
"""Linter to check that files in src/platforms/ either have 'namespace fl {' or an exception.

Files in src/platforms/**/*.{h,cpp} must either:
- Contain 'namespace fl {', OR
- Have a comment with '// ok no namespace fl' to opt out

This prevents namespace pollution from platform-specific implementations.
"""

import os
import unittest
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from typing import List, Tuple

from ci.util.paths import PROJECT_ROOT


SRC_ROOT = PROJECT_ROOT / "src"
PLATFORMS_DIR = SRC_ROOT / "platforms"

NUM_WORKERS = 1 if os.environ.get("NO_PARALLEL") else (os.cpu_count() or 1) * 4


def check_file_has_namespace_or_exception(file_path: str) -> Tuple[bool, str]:
    """
    Check if a file has 'namespace fl {' or an exception comment.

    Args:
        file_path (str): Path to the file to check

    Returns:
        Tuple[bool, str]: (is_valid, error_message)
    """
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            content = f.read()

        # Check for namespace fl
        if "namespace fl" in content:
            return True, ""

        # Check for exception comment
        if "// ok no namespace fl" in content:
            return True, ""

        # Neither found - violation
        return False, "Missing 'namespace fl {' or '// ok no namespace fl' comment"

    except (UnicodeDecodeError, IOError):
        # Skip files that can't be read
        return True, ""


class PlatformsFlNamespaceTester(unittest.TestCase):
    def test_platforms_have_namespace_or_exception(self) -> None:
        """Check that files in src/platforms/ have 'namespace fl' or exception comment."""
        files_to_check: List[str] = []

        # Find all .h, .cpp, and .hpp files in src/platforms/
        try:
            if PLATFORMS_DIR.exists():
                for file_path in PLATFORMS_DIR.rglob("*"):
                    if file_path.is_file() and file_path.suffix in [
                        ".h",
                        ".cpp",
                        ".hpp",
                    ]:
                        files_to_check.append(str(file_path))
        except (OSError, IOError):
            pass

        if not files_to_check:
            print("✅ No files found in src/platforms/")
            return

        # Check files in parallel
        violations: List[str] = []
        with ThreadPoolExecutor(max_workers=NUM_WORKERS) as executor:
            futures = {
                executor.submit(
                    check_file_has_namespace_or_exception, file_path
                ): file_path
                for file_path in files_to_check
            }
            for future in futures:
                file_path = futures[future]
                is_valid, error_msg = future.result()
                if not is_valid:
                    rel_path = os.path.relpath(file_path, PROJECT_ROOT)
                    violations.append(f"{rel_path}: {error_msg}")

        if violations:
            msg = f"Found {len(violations)} file(s) in src/platforms/ without 'namespace fl' or exception:\n"
            for violation in violations:
                print(violation)
            self.fail(
                msg
                + "\n".join(violations)
                + "\n\n💡 Files in src/platforms/**/*.{h,cpp} must either:\n"
                + "  1. Contain 'namespace fl {', OR\n"
                + "  2. Have '// ok no namespace fl' comment to opt out"
            )
        else:
            print(
                f"✅ All {len(files_to_check)} files in src/platforms/ have 'namespace fl' or exception"
            )


if __name__ == "__main__":
    unittest.main()
